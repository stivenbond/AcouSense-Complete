import pytest
from httpx import AsyncClient


@pytest.mark.asyncio
async def test_health(async_client: AsyncClient):
    response = await async_client.get("/health")
    assert response.status_code == 200
    assert response.json()["status"] == "ok"


@pytest.mark.asyncio
async def test_items_crud(async_client: AsyncClient):
    # Create
    item_payload = {
        "name": "Integration Test Item",
        "description": "desc",
        "timestamp": "2025-01-01T00:00:00+00:00",
    }
    resp = await async_client.post("/api/items", json=item_payload)
    assert resp.status_code == 201
    item_id = resp.json()["id"]

    # Get
    resp = await async_client.get(f"/api/items/{item_id}")
    assert resp.status_code == 200
    assert resp.json()["name"] == "Integration Test Item"

    # Update
    resp = await async_client.patch(f"/api/items/{item_id}", json={"name": "Updated"})
    assert resp.status_code == 200

    # Delete
    resp = await async_client.delete(f"/api/items/{item_id}")
    assert resp.status_code == 200
