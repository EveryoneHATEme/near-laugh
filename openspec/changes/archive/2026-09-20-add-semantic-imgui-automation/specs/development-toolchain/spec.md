## ADDED Requirements

### Requirement: Everyday semantic testing route and readiness evidence

All functional editor UI checks SHALL first use editor-ui MCP or its official SDK client, including newly prepared runtime scenarios without an existing test executable. The main agent SHALL prepare expectations and delegate the whole bounded scenario. Unavailable tools, failed scenarios and unsupported operations SHALL retain precise diagnostics without automatic Windows input fallback. Visual checks SHALL remain separately authorized and SHALL NOT be inferred from functional success.

#### Scenario: New feature has no regression yet
- **WHEN** a feature uses supported standard widgets
- **THEN** its readiness requires semantic metadata, independent applied-state assertions and a reproducible runtime regression without new feature-specific host handlers

#### Scenario: Doctor runs without launch permission
- **WHEN** an operator runs the ordinary diagnostic command
- **THEN** it checks exact Python/dependency prerequisites, fixed build/resource manifests and real official SDK initialization/tools-list without opening an editor or changing configuration/environment; untested desktop/GPU and build freshness remain `not_checked`

#### Scenario: Functional run is prepared
- **WHEN** a runner is assigned a semantic scenario
- **THEN** affected targets are incrementally built outside MCP before the run, known steps are batched within existing limits, fresh identities are discovered with scoped bounded observations, and transcript/summary retain statuses, failed step, expected/observed, effects, cleanup, counts, timing and evidence locations

#### Scenario: SDK client works but agent tools are absent
- **WHEN** a terminal SDK client initializes and executes successfully but the runner cannot see the four MCP tools
- **THEN** evidence distinguishes A no-window tests, B production-host/editor SDK integration and C agent-visible tool execution, leaves C blocked and explains any required Codex restart rather than claiming connection

#### Scenario: Operator adds a fixture
- **WHEN** a reviewed new level is registered in the bounded fixture manifest
- **THEN** it can be selected without host semantic changes while only simple filenames beneath the trusted packaged level directory are accepted; arbitrary executables, outside paths and user documents remain forbidden

### Requirement: Official SDK local MCP integration

The local MCP host SHALL use the official MCP SDK with pinned runtime dependencies. It SHALL expose the four existing generic semantic tools with typed input/output schemas and machine-readable results, forwarding complete execution batches to the editor without reproducing semantic resolution or widget execution in the host. Reproducible setup and a project-local Codex configuration template SHALL document supported settings using current official documentation, without changing global user configuration, agent models or committing machine-specific absolute paths.

#### Scenario: SDK client connects to the local host
- **WHEN** an official SDK client initializes, lists tools and calls a tool over stdio
- **THEN** the negotiated lifecycle, strict schemas and structured tool results interoperate without a handwritten MCP implementation, and stdout contains only protocol messages

#### Scenario: Real editor integration is validated
- **WHEN** SDK conformance checks succeed
- **THEN** acceptance additionally exercises SDK client through host and real editor to a UI action and observed applied result, including malformed requests, incompatible private protocol, child death and mid-batch failure; unavailable checks remain blocked

#### Scenario: Diagnostic output exceeds the response budget
- **WHEN** editor diagnostics or a tool result cannot fit the bounded inline response
- **THEN** bounded local artifacts retain available evidence and results identify their locations, without automatically returning the whole document/UI tree or replaying a mutation

### Requirement: Isolated opt-in UI automation build

The development workflow SHALL provide an explicitly enabled editor automation build using the project's existing compiler/ABI policy and a reproducibly pinned compatible automation dependency. Ordinary game/editor builds SHALL require no automation dependency fetch, contain no automation control endpoint and retain their normal dependency boundaries even when the automation variant is also built. All parts of an instrumented executable SHALL use a consistent UI-library configuration. Dependency license terms and required distribution notices SHALL be recorded without assuming that the automation dependency shares another dependency's license.

#### Scenario: Ordinary build is configured
- **WHEN** a developer configures and builds without enabling UI automation
- **THEN** no automation dependency is required or fetched and neither game nor editor exposes an automation endpoint

#### Scenario: Both editor variants are built
- **WHEN** the optional automation target and the ordinary editor are built in the same development configuration
- **THEN** the ordinary editor retains its normal behavior/dependency boundary and the automation executable uses only the matching instrumented UI-library configuration

#### Scenario: Pinned dependency is integrated
- **WHEN** a developer prepares the automation target from a fresh dependency checkout
- **THEN** the exact pinned revision, compiler/ABI compatibility check, license basis and required notices are reproducible and documented without upgrading unrelated dependencies

### Requirement: Separate functional and visual automation evidence

The documented workflow SHALL explain local session startup, fixture confinement, the structured observation/action/inspection tools, coverage limits and error recovery. It SHALL distinguish deterministic checks, actual MCP/editor integration, Vulkan lifecycle validation and authorized visual acceptance. Existing automated UI runs SHALL follow the repository's automated-test agent routing, and screenshot interaction SHALL follow its separate visual-driver routing and desktop authorization rules. Unavailable checks SHALL be reported explicitly; neither compilation nor structured functional success SHALL be described as visual acceptance.

#### Scenario: Developer validates a supported control
- **WHEN** a developer follows the structured UI validation workflow
- **THEN** they can retain runtime request/results and test outcomes proving the actual supported interaction without requiring screenshots or a feature-specific scenario implementation

#### Scenario: GPU or visual check is unavailable
- **WHEN** required hardware/desktop authorization or visual tools are unavailable
- **THEN** those checks are reported as blocked or not run separately from successful deterministic checks, without a silent fallback or a claim of full visual acceptance
