## Why

The player needs ways to obtain help and escape a familiar but dangerous
apartment. Early action must be able to improve the situation. The existing
planned interactions and cast need a coherent encounter that respects those
decisions and reaches an appropriate ending.

## What Changes

- Implement the concrete danger/escape phases needed for retreat to Lena's
  room, access to the kitchen/rear stairs, and the final request at a neighbor's
  door. Use the known routes and actor/door behavior rather than a generic AI
  framework.
- Compose telephone, window, and neighbor help attempts with explicit
  eligibility and consequences. An accepted early request changes or cancels
  incompatible confrontation/pursuit actions.
- Preserve the significance of the letter through recognition or response,
  without making it the only key to survival. Define alternate exploration
  order, repeated attempts, and route obstruction behavior.
- Represent the outcome for Lena and Anna Petrovna with concrete facts and
  select the corresponding authored epilogue, including narration/captions
  and the daylight corridor presentation using existing scene facilities.
- During design, resolve whether contact causes a retry and what determines
  the neighbor's outcome. If failure is needed, provide a clear authored
  condition and checkpoint return; do not invent health or damage.
- Extend checkpoint reconstruction and prepared playtest setups for encounter
  and ending state. Author and validate the supported help/exit/outcome links.
- No prolonged generic pursuit loop, combat, procedural search behavior, or
  hidden global countdown imposed solely to force a particular ending.

## Capabilities

### New Capabilities

- `escape-and-help-outcomes`: Concrete danger response, routes to help,
  decision-sensitive rescue outcomes, and corresponding epilogue entry.

### Modified Capabilities

- `level-persistence`: Persist supported encounter/help/outcome definitions
  and validate references against authored scene records.
- `level-object-placement`: Author concrete help and escape scene marks.
- `level-editor`: Configure and diagnose the bounded encounter/outcome links.
- `runtime-composition`: Coordinate encounter/ending transitions, actor and
  world state, presentation, and checkpoint reconstruction.

## Impact

Affects game-specific progression, visitor behavior, help interactions,
checkpoint reconstruction, episode setups, and final narrative presentation.
Update gameplay documentation with the chosen danger, failure, and outcome
rules. Temporary content is sufficient to validate complete routes.

## Dependencies and Boundaries

P08; requires [P07](../add-scripted-characters/proposal.md) and
[P11](../add-story-playtest-tools/proposal.md), including their household,
narrative, audio, door, and checkpoint prerequisites.
Final lighting is P10; full menus/distribution are P12.

## Acceptance Criteria

- Complete the ordinary kitchen/rear-stairs escape and reach the authored
  neighbor response and matching epilogue.
- Successfully request help early and observe a different compatible sequence
  with no later resurrection of cancelled danger.
- Exercise letter-delivered and letter-not-delivered runs; both retain a
  viable survival route. Vary supported errand/exploration order and repeats.
- Test route obstruction, leaving a conversation, and competing help/escape
  triggers without an unrecoverable progression trap.
- Resume the encounter and outcome checkpoints consistently. If design adopts
  failure, verify its feedback and short checkpoint retry.
- Run deterministic branch/reconstruction tests and play the full temporary
  story, including essential-clue comprehension with audio muted.
