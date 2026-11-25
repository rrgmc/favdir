# Changelog fragments

Every change that people who use the project would notice adds one file here, never an edit to
`CHANGELOG.md`. A release merges the fragments into `CHANGELOG.md` and deletes them.

- Name: `<branch or topic>.<type>.md`, where `type` is one of `added`, `changed`, `deprecated`,
  `removed`, `fixed`, `security` (the [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
  sections).
- Content: one or more bullets, written for people who use the project:
  `- Folders can be pinned to the top of a section.`
- A pull request without a user-visible change gets the label `no-changelog` instead.
