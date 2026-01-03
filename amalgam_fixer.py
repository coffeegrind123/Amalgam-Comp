#!/usr/bin/env python3
"""
Systematically fixes crash-prone code and optimizes the Amalgam codebase.
Uses SQLite database as single source of truth for tracking progress.
Spawns fresh Claude session for each file to avoid context accumulation.
"""

import json
import sqlite3
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Optional, Tuple

# Claude Sonnet 4 pricing (per 1M tokens)
INPUT_TOKEN_COST = 3.00
OUTPUT_TOKEN_COST = 15.00
CACHE_READ_TOKEN_COST = 0.30
CACHE_WRITE_TOKEN_COST = 3.75

# Paths
DB_PATH = Path(".amalgam_fixer.db")
CODEBASE_ROOT = Path("./Amalgam/src")

# File patterns to process
CPP_PATTERNS = ["*.cpp", "*.h", "*.hpp"]

PROMPT_TEMPLATE = """Fix and optimize this file: {filepath}

🎯 MISSION: Fix ALL crash-prone code and optimize performance while RETAINING ALL FUNCTIONALITY.

⚠️ CRITICAL RULES:
1. NEVER remove functionality - only fix and optimize
2. Add inline code comments explaining fixes and optimizations
3. Focus on null pointer safety, bounds checking, and performance
4. Do NOT create separate documentation files
5. Make ALL changes directly in the source file

🔧 NULL POINTER SAFETY FIXES (PRIORITY):
- Add nullptr checks BEFORE all pointer dereferences
- Validate GetClientEntity() results before calling ->As<>()
- Check handles before dereferencing: m_hActiveWeapon, m_hOwner, etc.
- Add nullptr guards for chained calls (obj->method()->otherMethod())
- Validate array/vector access with bounds checking
- Check function return values before using results
- Add early returns when pointers are null

Example patterns to fix:
```cpp
// BEFORE (crashes):
auto pVictim = I::ClientEntityList->GetClientEntity(iVictim)->As<CTFPlayer>();

// AFTER (safe):
auto pEntity = I::ClientEntityList->GetClientEntity(iVictim);
if (!pEntity) return; // FIXED: Null check prevents crash
auto pVictim = pEntity->As<CTFPlayer>();
if (!pVictim) return; // SAFETY: Validate cast result
```

💡 OPTIMIZATION (keep functionality):
- Cache repeated calculations (sqrt, expensive lookups)
- Use const& for large parameters instead of copies
- Mark read-only functions as const
- Use early returns to reduce nesting depth
- Replace O(n²) algorithms with O(n log n) or O(n)
- Remove redundant null checks if already validated above
- Use initializer lists instead of assignment in constructors
- Prefer stack allocation over heap when safe
- Inline frequently called small functions
- Use std::move for large objects when ownership transfers

📝 INLINE DOCUMENTATION:
Add comments directly in code:
```cpp
// FIXED: Null check added to prevent crash when entity invalid
// OPTIMIZED: Cached matrix calculation to avoid repeated inverse()
// SAFETY: Bounds check prevents vector out-of-range crash
```

🚫 DO NOT:
- Remove existing features or game logic
- Change function signatures unless absolutely necessary
- Add external library dependencies
- Create .md files or separate documentation
- Break existing code that calls these functions
- Make "improvements" that could introduce new bugs

✅ VALIDATION CHECKLIST:
- [ ] All pointer dereferences have null checks
- [ ] All array/vector accesses have bounds checks
- [ ] All optimizations preserve exact same behavior
- [ ] Code comments explain WHAT was fixed/optimized
- [ ] No functionality was removed or changed
- [ ] Changes compile without errors
- [ ] Original logic flow is preserved

After completing ALL fixes and optimizations:
1. Make ONE git commit: "Fix crashes and optimize {filename}"
2. Push the commit
3. Respond with: "COMPLETED: {filepath}"

Start by reading the file, then systematically fix and optimize it.
"""


class DatabaseManager:
    """Manages SQLite database for tracking file processing."""
    
    def __init__(self, db_path: Path):
        self.db_path = db_path
        self.conn = None
        self.init_database()
    
    def init_database(self):
        """Initialize database schema."""
        self.conn = sqlite3.connect(str(self.db_path))
        self.conn.execute("""
            CREATE TABLE IF NOT EXISTS files (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                filepath TEXT UNIQUE NOT NULL,
                file_size INTEGER,
                last_modified REAL,
                status TEXT DEFAULT 'pending',
                processing_started REAL,
                processing_completed REAL,
                attempts INTEGER DEFAULT 0,
                tokens_input INTEGER DEFAULT 0,
                tokens_output INTEGER DEFAULT 0,
                tokens_cache_read INTEGER DEFAULT 0,
                tokens_cache_write INTEGER DEFAULT 0,
                cost REAL DEFAULT 0,
                error_message TEXT,
                created_at REAL DEFAULT (julianday('now'))
            )
        """)
        
        self.conn.execute("""
            CREATE INDEX IF NOT EXISTS idx_status ON files(status)
        """)
        self.conn.execute("""
            CREATE INDEX IF NOT EXISTS idx_size ON files(file_size)
        """)
        
        self.conn.execute("""
            CREATE TABLE IF NOT EXISTS global_stats (
                id INTEGER PRIMARY KEY CHECK (id = 1),
                total_files INTEGER DEFAULT 0,
                files_completed INTEGER DEFAULT 0,
                files_failed INTEGER DEFAULT 0,
                files_skipped INTEGER DEFAULT 0,
                total_input_tokens INTEGER DEFAULT 0,
                total_output_tokens INTEGER DEFAULT 0,
                total_cache_read_tokens INTEGER DEFAULT 0,
                total_cache_write_tokens INTEGER DEFAULT 0,
                total_cost REAL DEFAULT 0,
                start_time REAL DEFAULT (julianday('now')),
                last_update REAL DEFAULT (julianday('now'))
            )
        """)
        
        # Initialize global stats if not exists
        self.conn.execute("""
            INSERT OR IGNORE INTO global_stats (id) VALUES (1)
        """)
        
        self.conn.commit()
    
    def get_next_file(self) -> Optional[Tuple[int, str, int]]:
        """Get next pending file to process (smallest first)."""
        cursor = self.conn.execute("""
            SELECT id, filepath, file_size FROM files 
            WHERE status = 'pending'
            ORDER BY file_size ASC
            LIMIT 1
        """)
        result = cursor.fetchone()
        return result if result else None
    
    def mark_started(self, file_id: int):
        """Mark file processing as started."""
        self.conn.execute("""
            UPDATE files 
            SET status = 'processing', 
                processing_started = julianday('now'),
                attempts = attempts + 1
            WHERE id = ?
        """, (file_id,))
        self.conn.commit()
    
    def mark_completed(self, file_id: int, tokens_in: int, tokens_out: int, 
                      tokens_cache_r: int, tokens_cache_w: int):
        """Mark file as successfully completed."""
        cost = (
            (tokens_in / 1_000_000) * INPUT_TOKEN_COST +
            (tokens_out / 1_000_000) * OUTPUT_TOKEN_COST +
            (tokens_cache_r / 1_000_000) * CACHE_READ_TOKEN_COST +
            (tokens_cache_w / 1_000_000) * CACHE_WRITE_TOKEN_COST
        )
        
        self.conn.execute("""
            UPDATE files 
            SET status = 'completed',
                processing_completed = julianday('now'),
                tokens_input = ?,
                tokens_output = ?,
                tokens_cache_read = ?,
                tokens_cache_write = ?,
                cost = ?,
                error_message = NULL
            WHERE id = ?
        """, (tokens_in, tokens_out, tokens_cache_r, tokens_cache_w, cost, file_id))
        
        self.conn.execute("""
            UPDATE global_stats 
            SET files_completed = files_completed + 1,
                total_input_tokens = total_input_tokens + ?,
                total_output_tokens = total_output_tokens + ?,
                total_cache_read_tokens = total_cache_read_tokens + ?,
                total_cache_write_tokens = total_cache_write_tokens + ?,
                total_cost = total_cost + ?,
                last_update = julianday('now')
        """, (tokens_in, tokens_out, tokens_cache_r, tokens_cache_w, cost))
        
        self.conn.commit()
    
    def mark_failed(self, file_id: int, error_msg: str):
        """Mark file as failed."""
        self.conn.execute("""
            UPDATE files 
            SET status = 'failed',
                error_message = ?
            WHERE id = ?
        """, (error_msg[:500], file_id))
        
        self.conn.execute("""
            UPDATE global_stats 
            SET files_failed = files_failed + 1,
                last_update = julianday('now')
        """)
        
        self.conn.commit()
    
    def get_stats(self) -> dict:
        """Get comprehensive statistics."""
        cursor = self.conn.execute("""
            SELECT 
                total_files,
                files_completed,
                files_failed,
                files_skipped,
                total_input_tokens,
                total_output_tokens,
                total_cache_read_tokens,
                total_cache_write_tokens,
                total_cost,
                (julianday('now') - start_time) * 24 as hours_elapsed
            FROM global_stats
            WHERE id = 1
        """)
        row = cursor.fetchone()
        
        if row:
            total, completed, failed, skipped, inp, out, cache_r, cache_w, cost, hours = row
            pending = total - (completed + failed + skipped)
            total_tokens = inp + out + cache_r + cache_w
            
            return {
                "total_files": total,
                "files_completed": completed,
                "files_failed": failed,
                "files_pending": pending,
                "hours_elapsed": hours,
                "total_cost": cost,
                "total_tokens": total_tokens,
                "tokens_per_hour": total_tokens / max(hours, 0.01),
                "cost_per_hour": cost / max(hours, 0.01),
                "avg_cost_per_file": cost / max(completed, 1)
            }
        
        return {}
    
    def close(self):
        """Close database connection."""
        if self.conn:
            self.conn.close()


def parse_tokens_from_output(lines: list) -> Tuple[int, int, int, int]:
    """Parse token usage from all JSON output lines."""
    total_input = 0
    total_output = 0
    total_cache_read = 0
    total_cache_write = 0
    
    for line in lines:
        try:
            data = json.loads(line.strip())
            usage = data.get("usage", {})
            
            total_input += usage.get("input_tokens", 0)
            total_output += usage.get("output_tokens", 0)
            total_cache_read += usage.get("cache_read_input_tokens", 0)
            
            cache_creation = usage.get("cache_creation", {})
            total_cache_write += cache_creation.get("ephemeral_5m_input_tokens", 0)
            total_cache_write += cache_creation.get("ephemeral_1h_input_tokens", 0)
        except (json.JSONDecodeError, ValueError, KeyError):
            continue
    
    return (total_input, total_output, total_cache_read, total_cache_write)


def is_completed(lines: list) -> bool:
    """Check if the file was successfully completed."""
    for line in reversed(lines):
        try:
            data = json.loads(line.strip())
            if "result" in data:
                result = data["result"]
                if "COMPLETED:" in result.upper():
                    return True
        except (json.JSONDecodeError, ValueError, KeyError):
            continue
    return False


def process_file(filepath: str, filename: str) -> Tuple[bool, list, str]:
    """Process a single file in a fresh Claude session."""
    prompt = PROMPT_TEMPLATE.format(filepath=filepath, filename=filename)
    
    cmd = [
        "/usr/local/bin/claude",
        "--dangerously-skip-permissions",
        "--verbose",
        "--output-format", "stream-json",
        "-p", prompt
    ]
    
    print(f"  Starting fresh session...", file=sys.stderr, flush=True)
    
    process = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    
    output_lines = []
    for line in process.stdout:
        output_lines.append(line.strip())
        if "result" in line:
            print(".", end="", file=sys.stderr, flush=True)
    
    process.wait()
    print("", file=sys.stderr)
    
    error_msg = ""
    if process.returncode != 0:
        error_msg = f"Process exited with code {process.returncode}"
    
    success = is_completed(output_lines) and process.returncode == 0
    
    return success, output_lines, error_msg


def main():
    """Main execution loop."""
    print("🔧 Amalgam Codebase Fixer & Optimizer", file=sys.stderr)
    print("=" * 80, file=sys.stderr)
    print("Fresh Claude session per file | SQLite progress tracking", file=sys.stderr)
    print("=" * 80, file=sys.stderr, flush=True)
    
    print(f"\n📊 Database: {DB_PATH}", file=sys.stderr, flush=True)
    db = DatabaseManager(DB_PATH)
    
    stats = db.get_stats()
    print(f"\n📁 Total files: {stats['total_files']}", file=sys.stderr)
    print(f"   Pending: {stats['files_pending']}", file=sys.stderr)
    print(f"   Completed: {stats['files_completed']}", file=sys.stderr)
    print(f"   Failed: {stats['files_failed']}", file=sys.stderr, flush=True)
    
    while True:
        next_file = db.get_next_file()
        
        if not next_file:
            print("\n✅ All files processed!", file=sys.stderr, flush=True)
            break
        
        file_id, filepath, file_size = next_file
        filename = Path(filepath).name
        
        stats = db.get_stats()
        progress_pct = (stats['files_completed'] / max(stats['total_files'], 1)) * 100
        
        print(f"\n{'='*80}", file=sys.stderr)
        print(f"[{stats['files_completed']}/{stats['total_files']} | {progress_pct:.1f}%] {filepath}", file=sys.stderr)
        print(f"Size: {file_size:,} bytes | ${stats['total_cost']:.2f} spent | {stats['hours_elapsed']:.1f}h", file=sys.stderr)
        print(f"{'='*80}", file=sys.stderr, flush=True)
        
        db.mark_started(file_id)
        
        try:
            success, output_lines, error_msg = process_file(filepath, filename)
            
            tokens_in, tokens_out, tokens_cache_r, tokens_cache_w = parse_tokens_from_output(output_lines)
            
            if success:
                db.mark_completed(file_id, tokens_in, tokens_out, tokens_cache_r, tokens_cache_w)
                cost = (
                    (tokens_in / 1_000_000) * INPUT_TOKEN_COST +
                    (tokens_out / 1_000_000) * OUTPUT_TOKEN_COST +
                    (tokens_cache_r / 1_000_000) * CACHE_READ_TOKEN_COST +
                    (tokens_cache_w / 1_000_000) * CACHE_WRITE_TOKEN_COST
                )
                print(f"✅ COMPLETED | {tokens_in + tokens_out:,} tokens | ${cost:.4f}", 
                      file=sys.stderr, flush=True)
            else:
                db.mark_failed(file_id, error_msg or "Did not complete successfully")
                print(f"❌ FAILED | {error_msg}", file=sys.stderr, flush=True)
        
        except KeyboardInterrupt:
            print("\n\n⚠️  Interrupted - progress saved", file=sys.stderr)
            db.close()
            sys.exit(1)
        
        except Exception as e:
            db.mark_failed(file_id, f"Exception: {str(e)}")
            print(f"💥 ERROR: {e}", file=sys.stderr, flush=True)
        
        time.sleep(1)
    
    final = db.get_stats()
    print("\n" + "=" * 80, file=sys.stderr)
    print("🎉 FINAL RESULTS", file=sys.stderr)
    print("=" * 80, file=sys.stderr)
    print(f"Total files: {final['total_files']}", file=sys.stderr)
    print(f"✅ Completed: {final['files_completed']}", file=sys.stderr)
    print(f"❌ Failed: {final['files_failed']}", file=sys.stderr)
    print(f"⏱️  Time: {final['hours_elapsed']:.2f} hours", file=sys.stderr)
    print(f"💰 Cost: ${final['total_cost']:.2f}", file=sys.stderr)
    print(f"📊 Tokens: {final['total_tokens']:,}", file=sys.stderr)
    print(f"📈 Rate: ${final['cost_per_hour']:.2f}/hr | {final['tokens_per_hour']:,.0f} tok/hr", file=sys.stderr)
    print(f"📄 Avg: ${final['avg_cost_per_file']:.4f}/file", file=sys.stderr)
    print("=" * 80, file=sys.stderr, flush=True)
    
    db.close()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted - all progress saved to database", file=sys.stderr)
        sys.exit(1)
