# Guidance for Claude Code

Claude Code auto-loads this file at the start of every session.
It does not add new rules;
it points at the ones every contributor already follows,
so they are in context before any code, commit or pull request.

Read and follow @AGENTS.md and @CONTRIBUTING.md.

Rules that are easy to miss when working as an AI agent:

- No `Co-authored-by:` lines, and no LLM or AI attribution of any kind,
  in commit messages or in pull request descriptions.
- Commit subject: imperative mood, <= 50 chars, capitalized, no trailing period.
- Wrap commit messages, comments and prose at clause boundaries,
  not at a fixed column.
- Code must build and pass the headless tests (`./tests/headless/run.sh`) before you push,
  and a change is verified by running it, not only by reading the diff.
- Don't force-push a published commit;
  a new change is a new commit.
