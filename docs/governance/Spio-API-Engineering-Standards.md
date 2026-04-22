# Spio API Engineering Standards

**Purpose:** Freeze the engineering standards used for public `spio` HTTP contract packages so frontend and backend teams can develop independently against the same interface artifacts.

**Last updated:** 2026-04-21

## External References

`spio` API contract work follows these external references:

1. OpenAPI Specification `3.1.0`
   https://spec.openapis.org/oas/v3.1.0
2. Learn OpenAPI guidance on reusable components
   https://learn.openapis.org/specification/components.html
3. Learn OpenAPI guidance on links between operations
   https://learn.openapis.org/specification/links.html
4. Arazzo Specification for multi-operation workflow descriptions
   https://spec.openapis.org/arazzo/latest.html
5. Microsoft REST API Guidelines
   https://github.com/microsoft/api-guidelines
6. GitHub REST API breaking-change guidance
   https://docs.github.com/en/rest/about-the-rest-api/breaking-changes

## Required Local Rules

1. Public HTTP APIs ship as versioned OpenAPI `3.1` contract packages under `contracts/`.
2. Multi-step frontend/backend workflows ship alongside the API as versioned Arazzo workflow documents.
3. Reusable request, response, schema, and example payloads must live in reusable components instead of being repeated inline.
4. `operationId` values are stable integration identifiers and must remain unique within a contract version.
5. Additive optional fields are allowed inside a stable major version; removing operations, renaming fields, changing required fields, or changing enum meaning requires a new version.
6. Contract packages must be linted and checked for drift in CI; human-readable docs explain the package but do not replace it.
7. Consumer repos bind to published contracts and examples, not to private source layout or unpublished payloads.
