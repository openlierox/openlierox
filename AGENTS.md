# AI agents

Follow [CONTRIBUTING.md](CONTRIBUTING.md), the same as any contributor.

## Running shell commands

Sandboxed agent tooling (such as Claude Code) auto-approves a command
only when it can statically prove the command is safe;
anything it cannot parse falls back to a manual confirmation prompt.
To keep commands auto-approved, write them plain and literal:

- No variable or command substitution (`$VAR`, `` $(...) ``);
  hardcode the value instead.
- No wildcards or brace expansion (`*.cpp`, `build*`, `{a,b}`);
  they expand to whatever is on disk, so pass the exact paths.
- No `for`/`while` loops;
  run separate commands, or issue parallel tool calls.
- Don't combine `cd` with a write in one compound command
  (`cd dir && git commit ...`);
  the working directory already persists between calls,
  so use an absolute path or `git -C <dir> ...`.
- Don't use `find -exec`;
  list with `find`, then act in a separate step.
- Don't pass a shell script as a quoted string (`sh -c "..."`).
- For multi-line text (a commit message, a file body),
  write it to a file and pass the file
  (`git commit -F msg.txt`, `gh pr create --body-file body.md`),
  instead of an inline heredoc or a multi-line quoted argument;
  a newline followed by `#` inside a quoted argument also trips the checker.
