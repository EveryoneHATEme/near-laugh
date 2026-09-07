## Why

After technical readiness, the game needs an authored story that uses the
supported interactions, characters, events, and recovery tools. This deferred
proposal retains earlier escape/help ideas for that discussion; it does not
establish the game's plot or block technical work.

## Planning Status

P08 belongs to story development after roadmap T6 is accepted. The named
characters, routes, letter, telephone/help decisions, and endings below are
draft options, not approved product requirements or technical acceptance
fixtures. Review the actual story scope and replace these candidate decisions
and checks before creating design, delta specs, or implementation tasks.

## What Changes

Candidate content to revisit during that later review:

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

Provisional capability scope, subject to the story review:

- `escape-and-help-outcomes`: Concrete danger response, routes to help,
  decision-sensitive rescue outcomes, and corresponding epilogue entry.

### Modified Capabilities

Candidate affected capabilities; re-evaluate against the implemented technical
profile. Do not add new serialized fields or mechanics if existing authoring
already expresses the selected story.

- `level-persistence`: Persist supported encounter/help/outcome definitions
  and validate references against authored scene records.
- `level-object-placement`: Author concrete help and escape scene marks.
- `level-editor`: Configure and diagnose the bounded encounter/outcome links.
- `runtime-composition`: Coordinate encounter/ending transitions, actor and
  world state, presentation, and checkpoint reconstruction.

## Impact

A selected version of this draft could affect progression, visitor behavior,
help interactions, checkpoint reconstruction, episode setups, and final
narrative presentation.
Update gameplay documentation only with subsequently chosen danger, failure,
and outcome rules. Technical readiness is validated separately on neutral scenes.

## Dependencies and Boundaries

P08 is deferred until T6 technical readiness, including completed
[P12](../add-game-session-and-packaging/proposal.md) and its P11 authoring
workflow gate, then an explicit review of story scope. Lighting, characters,
interactions, events, checkpoints, menus, diagnostics, and packaging precede
this work. P08 is not a prerequisite for any technical proposal.

## Acceptance Criteria

Deferred candidate checks only. Replace them with acceptance for the selected
story after scope review; neither route nor ending is committed by this plan:

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
