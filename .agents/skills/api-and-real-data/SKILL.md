---
name: api-and-real-data
description: Traces API-backed features end to end and prevents mock data, fake authentication, or unsupported API claims from being presented as real.
---

# API and Real Data Integrity

1. Trace UI -> state/controller -> repository/service -> HTTP client -> official API -> response mapping.
2. Identify the actual documented endpoint, permission, scopes, authentication method, quotas and limitations.
3. Distinguish real API data, cache, local persistence, mock fixtures and placeholders.
4. Never claim a feature works with a production service based only on unit tests using fake responses.
5. Do not invent endpoints or assume one API permission grants access to unrelated private data.
6. Never log or expose passwords, access tokens, session cookies, OTPs or secrets.
7. Handle expiry, denied permission, rate limits, network errors and empty responses explicitly.
8. If official API access does not support the requested behavior, report BLOCKED and explain the limitation; do not bypass access controls.
9. Test parsing with fixtures, then test integration only when valid credentials and permissions are available.
