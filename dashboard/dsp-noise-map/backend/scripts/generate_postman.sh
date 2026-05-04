#!/usr/bin/env bash
set -e

# Wait for FastAPI to start and dump the openapi.json
curl -s http://localhost:8000/openapi.json -o openapi.json

# Transform the OpenAPI spec to Postman Collection v2.1
npx openapi-to-postmanv2 -s openapi.json -o postman_collection.json

echo "Postman collection generated successfully at postman_collection.json"
