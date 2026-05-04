"""initial

Revision ID: 001
Revises:
Create Date: 2025-01-01 00:00:00.000000
"""

import sqlalchemy as sa
from alembic import op
from sqlalchemy.engine.reflection import Inspector

revision = "001"
down_revision = None
branch_labels = None
depends_on = None


def upgrade() -> None:
    conn = op.get_bind()
    inspector = Inspector.from_engine(conn)
    tables = inspector.get_table_names()

    if "items" not in tables:
        op.create_table(
            "items",
            sa.Column("id", sa.Integer, primary_key=True),
            sa.Column("name", sa.String(255), nullable=False),
            sa.Column("description", sa.Text(), nullable=True),
            sa.Column("timestamp", sa.String(50), nullable=False),
            sa.Column("deleted_at", sa.String(50), nullable=True),
        )
        op.create_index("ix_items_deleted_at", "items", ["deleted_at"])

    if "commands" not in tables:
        op.create_table(
            "commands",
            sa.Column("id", sa.Integer, primary_key=True),
            sa.Column("operation_type", sa.String(50), nullable=False),
            sa.Column("payload_json", sa.Text(), nullable=False),
            sa.Column("executed_at", sa.String(50), nullable=False),
            sa.Column("undone_at", sa.String(50), nullable=True),
            sa.Column("session_id", sa.String(100), nullable=True),
        )
        op.create_index("ix_commands_session", "commands", ["session_id"])


def downgrade() -> None:
    op.drop_table("commands")
    op.drop_table("items")
