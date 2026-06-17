Audit all changed or specified files for British English spelling enforcement.

## Rules

All documentation, comments, and user-facing strings MUST use British English spelling
(colour, initialise, behaviour, centre, optimise, recognise, and so on).

## Common Violations to Check

| American (wrong) | British (correct) |
|-------------------|-------------------|
| color | colour |
| behavior | behaviour |
| optimize | optimise |
| center | centre |
| license (noun) | licence |
| meter | metre |
| synchronize | synchronise |
| initialize | initialise |
| analyze | analyse |
| organize | organise |
| recognize | recognise |
| customize | customise |
| utilize | utilise |
| minimize | minimise |
| maximize | maximise |
| normalize | normalise |
| authorize | authorise |
| serialize | serialise |
| finalize | finalise |
| paralyze | paralyse |
| catalog | catalogue |
| dialog | dialogue |
| gray | grey |
| defense | defence |
| offense | offence |
| pretense | pretence |
| fulfill | fulfil |
| enrollment | enrolment |
| modeling | modelling |
| traveling | travelling |
| canceled | cancelled |
| labeled | labelled |

## Exceptions

- **Code identifiers** may use American spelling where it matches library/API conventions (for example Vulkan API names like
  `VkColorSpaceKHR`, or Win32 names). Do not "correct" these.
- **`GNU General Public License`** — proper noun, keep the American "License" spelling. British "licence" applies only to ordinary prose such as "the licence file".
- **`LICENCE`** — this is the British filename of the licence file; it is off limits as a legal document, do not modify its contents.
- **`CODE_OF_CONDUCT.md`** — off limits, do not modify.
- **External library names** (for example `volk`) are proper nouns.
- **Quoted text** from external sources.

## What to Check

If no files are specified via $ARGUMENTS, check all `.md`, `.cpp`, `.hpp`, and `.yml` files in the repo.
Report every violation with file path, line number, the offending word, and its British equivalent.
Then offer to fix them all.
