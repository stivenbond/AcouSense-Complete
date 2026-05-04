package com.acousense.data.db

import androidx.annotation.VisibleForTesting
import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import kotlinx.coroutines.flow.Flow

@Dao
interface SyncSessionDao {
    @VisibleForTesting
    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insert(session: SyncSession): Long

    @VisibleForTesting
    @Query("SELECT * FROM sync_sessions WHERE periodStart >= :start AND periodEnd <= :end ORDER BY periodStart ASC")
    suspend fun getByDateRange(start: Long, end: Long): List<SyncSession>

    @VisibleForTesting
    @Query("SELECT * FROM sync_sessions ORDER BY syncedAt DESC LIMIT :limit")
    fun getRecentSessions(limit: Int): Flow<List<SyncSession>>

    @VisibleForTesting
    @Query("SELECT * FROM sync_sessions ORDER BY syncedAt DESC LIMIT 1")
    fun getLatestSession(): Flow<SyncSession?>

    @VisibleForTesting
    @Query("DELETE FROM sync_sessions WHERE periodEnd < :cutoff")
    suspend fun deleteOlderThan(cutoff: Long)

    @VisibleForTesting
    @Query("DELETE FROM sync_sessions")
    suspend fun deleteAll()

    @VisibleForTesting
    @Query("SELECT COUNT(*) FROM sync_sessions")
    suspend fun countAll(): Int
}
