Application Programming Interface.

At a practical level for web development, an API is a contract between a client and a server:

Client sends a request.
Server validates, processes, and returns a response.
Both sides agree on structure, status codes, and behavior.
For HTTP APIs, the contract includes:

URL path.
HTTP method.
Request headers.
Request body format.
Response status code.
Response body format.
Error format.

Simple Mental Model
Think of an API like a restaurant:

Menu = documented endpoints.
Waiter = HTTP protocol.
Kitchen = server business logic.
Order format = request schema.
Food delivered = response schema.
If the menu says pasta and you order pasta, you expect pasta every time. That predictability is the core of API design.

Core API Structure
A clean API is usually organized into these layers:

Transport layer.
Handles raw HTTP details: socket, parsing request line/headers/body, serializing response.

Routing layer.
Maps method + path to handler.
Example idea: GET /api/users goes to getUsersHandler.

Validation layer.
Checks required fields, types, constraints, and auth before business logic.

Business logic layer.
Implements real behavior: create user, fetch file, apply rules.

Data layer.
Reads and writes data: files, DB, external services.

Error mapping layer.
Converts internal errors to stable HTTP errors and JSON error bodies.

Documentation layer.
Defines endpoint contract so clients know how to use it.

Example endpoint contract for GET /api/data:

Request.
Method: GET.
Path: /api/data.
Headers: Accept: application/json.

Success response.
Status: 200.
Body:
{
"items": [...]
}

Failure response.
Status: 404 if resource missing.
Status: 403 if forbidden.
Status: 500 for unexpected server failure.

REST-Style API Design (Most Common)
REST is not a strict protocol, but a style:

Use nouns in URLs, not verbs.
Use HTTP methods to express action.
Keep endpoints predictable.
Keep responses consistent.
Common mapping:

GET /users -> list.
GET /users/42 -> read one.
POST /users -> create.
PUT /users/42 -> replace.
PATCH /users/42 -> partial update.
DELETE /users/42 -> delete.

Status Code Strategy
Use codes semantically:

200/201/204 for success.
400 for malformed request.
401 unauthenticated.
403 authenticated but forbidden.
404 not found.
409 conflict.
422 validation failed.
500 unexpected internal error.
503 temporary service unavailable.

Versioning Strategy
Versioning protects clients from breaking changes:

URL versioning: /api/v1/users.
Header versioning: Accept: application/vnd.myapp.v1+json.
Most teams start with URL versioning because it is simple and explicit.

Authentication and Authorization
These are different:

Authentication: who are you.
Authorization: what can you do.
Typical flow:

Client sends token in Authorization header.
Server verifies token.
Server checks permissions for endpoint/resource.
Return 401 or 403 as needed.

Pagination, Filtering, Sorting
For list endpoints:

page and page_size or cursor.
filter params like status=active.
sort params like sort=-created_at.
Return metadata with count/next cursor.
Without this, list endpoints become slow and hard to use.

Idempotency and Safety
Important HTTP behavior:

GET should be safe and not mutate.
PUT and DELETE should be idempotent.
POST is usually non-idempotent.
Use idempotency keys for payment-like operations.

API Documentation
Always document:

Endpoint path and method.
Request schema and examples.
Response schema and examples.
Status codes and error schema.
Auth requirements.
Rate limits and retries.
Good docs turn APIs into products.

