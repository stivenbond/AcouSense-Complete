package com.acousense.data.db

import androidx.compose.runtime.Immutable
import androidx.room.Entity
import androidx.room.PrimaryKey

@Immutable
@Entity(tableName = "sync_sessions")
data class SyncSession(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val userIdentifier: String,
    val periodStart: Long,
    val periodEnd: Long,
    val avgLevel: Int,
    val maxLevel: Int,
    val peakCount: Int,
    val exposureSeconds: Long,
    val classification: Int,
    val syncedAt: Long = System.currentTimeMillis()
)
