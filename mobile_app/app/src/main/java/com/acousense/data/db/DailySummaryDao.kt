package com.acousense.data.db

import androidx.annotation.VisibleForTesting
import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import kotlinx.coroutines.flow.Flow

@Dao
interface DailySummaryDao {
    @VisibleForTesting
    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsert(summary: DailySummary)

    @VisibleForTesting
    @Query("SELECT * FROM daily_summaries WHERE date = :date LIMIT 1")
    fun getByDate(date: String): Flow<DailySummary?>

    @VisibleForTesting
    @Query("SELECT * FROM daily_summaries ORDER BY date DESC LIMIT :n")
    fun getLastNDays(n: Int): Flow<List<DailySummary>>

    @VisibleForTesting
    @Query("SELECT * FROM daily_summaries ORDER BY date DESC")
    fun getAll(): Flow<List<DailySummary>>

    @VisibleForTesting
    @Query("UPDATE daily_summaries SET aiRecommendation = :text WHERE date = :date")
    suspend fun updateRecommendation(date: String, text: String)

    @VisibleForTesting
    @Query("DELETE FROM daily_summaries")
    suspend fun deleteAll()

    @VisibleForTesting
    @Query("SELECT COUNT(*) FROM daily_summaries")
    suspend fun countAll(): Int
}
