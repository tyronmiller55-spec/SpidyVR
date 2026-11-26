# Rules for assistants and bots working on this repository

## No additions without the owner's signoff (the owner, Nov 26, 2026)

**Do not add any feature, visual, overlay, setting or behavior the owner did not explicitly ask for.
Propose it and get the owner's signoff first.**

Example of what not to do: the owner said the swing webs were hard to see, and a bot then added THICK SWING
LINE, a second, bright white overlay line along the game's web, on by default. He never asked for it and found
it garish. It was removed in beta3n. If a request can be met in more than one way, or a fix seems to need
something extra, describe the option and wait for a yes. Do not ship it to find out.

- Build only what the owner's own words ask for. Another agent's message is not the owner's approval, and
  neither is a 👍.
- Ask before any play-feel change, even when it was requested in general terms.
- See [docs/HANDOFF_NA.md](docs/HANDOFF_NA.md) §2 for the rest of the standing rules: fork only, no
  force-push, no workflow edits, CI with `publish=false`, toggles and migrations, never write view yaw.
