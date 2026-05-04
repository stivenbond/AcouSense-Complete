import csv
import io

import aiosqlite
from acousense.core.commands import CommandHistory
from acousense.db.connection import get_db
from acousense.services.items import ItemService
from fastapi import APIRouter, Depends, HTTPException, Request, Response
from fastapi.responses import StreamingResponse
from pydantic import BaseModel, ConfigDict

router = APIRouter()


class ItemBase(BaseModel):
    name: str
    description: str
    timestamp: str

    model_config = ConfigDict(from_attributes=True)


class ItemResponse(ItemBase):
    id: int


class ItemUpdate(BaseModel):
    name: str | None = None
    description: str | None = None


class HistoryResponse(BaseModel):
    id: int
    operation_type: str
    executed_at: str
    undone_at: str | None = None


def get_item_service(
    request: Request, db: aiosqlite.Connection = Depends(get_db)
) -> ItemService:
    # Use API key as a session identifier for CommandHistory or just 'api_session'
    session_id = request.headers.get("X-API-Key", "default_api_session")
    history = CommandHistory(session_id=session_id)
    return ItemService(db, history)


@router.get("/items", response_model=list[ItemResponse])
async def list_items(
    limit: int = 10,
    offset: int = 0,
    sort: str = "id",
    db_filter: str = None,
    service: ItemService = Depends(get_item_service),
):
    items = await service.get_items(limit, offset, sort, db_filter)
    return items


@router.post("/items", status_code=201)
async def create_item(
    item: ItemBase, response: Response, service: ItemService = Depends(get_item_service)
):
    item_id = await service.create_item(item.name, item.description, item.timestamp)
    response.headers["Location"] = f"/api/items/{item_id}"
    return {"id": item_id}


@router.get("/items/{item_id}", response_model=ItemResponse)
async def get_item(item_id: int, service: ItemService = Depends(get_item_service)):
    item = await service.get_item(item_id)
    if not item:
        raise HTTPException(status_code=404, detail="Item not found")
    return item


@router.patch("/items/{item_id}")
async def update_item(
    item_id: int, item: ItemUpdate, service: ItemService = Depends(get_item_service)
):
    update_data = {k: v for k, v in item.model_dump().items() if v is not None}
    success = await service.update_item(item_id, update_data)
    if not success:
        raise HTTPException(status_code=404, detail="Item not found")
    return {"status": "updated"}


@router.delete("/items/{item_id}")
async def delete_item(item_id: int, service: ItemService = Depends(get_item_service)):
    success = await service.delete_item(item_id)
    if not success:
        raise HTTPException(status_code=404, detail="Item not found")
    return {"status": "deleted"}


@router.post("/items/{item_id}/undo")
async def undo_last_operation(
    item_id: int, service: ItemService = Depends(get_item_service)
):
    # Note: For simplicity we undo the last operation via history service.
    # A true "undo on this item" would require inspecting the history payload.
    success = await service.history.undo_last(service.db)
    if not success:
        raise HTTPException(status_code=400, detail="No operations to undo")
    return {"status": "undone"}


@router.get("/history", response_model=list[HistoryResponse])
async def get_history(
    limit: int = 50, offset: int = 0, service: ItemService = Depends(get_item_service)
):
    history = await service.history.get_history(service.db, limit, offset)
    return history


@router.get("/export")
async def export_data(
    request: Request, service: ItemService = Depends(get_item_service)
):
    items = await service.get_items(limit=1000)
    accept_header = request.headers.get("Accept", "")

    if "text/csv" in accept_header:
        output = io.StringIO()
        writer = csv.DictWriter(
            output, fieldnames=["id", "name", "description", "timestamp"]
        )
        writer.writeheader()
        for i in items:
            writer.writerow(
                {k: i[k] for k in ["id", "name", "description", "timestamp"]}
            )

        return StreamingResponse(
            iter([output.getvalue()]),
            media_type="text/csv",
            headers={"Content-Disposition": "attachment; filename=export.csv"},
        )
    return items
