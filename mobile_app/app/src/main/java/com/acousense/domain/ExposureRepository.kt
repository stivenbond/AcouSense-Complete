package com.acousense.domain

import com.acousense.data.db.DailySummary
import com.acousense.data.db.DailySummaryDao
import com.acousense.data.db.SyncSession
import com.acousense.data.db.SyncSessionDao
import kotlinx.coroutines.flow.Flow
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class ExposureRepository @Inject constructor(
    private val syncSessionDao: SyncSessionDao,
    private val dailySummaryDao: DailySummaryDao
) {
    suspend fun insertSession(session: SyncSession): Long = syncSessionDao.insert(session)
    fun latestSession(): Flow<SyncSession?> = syncSessionDao.getLatestSession()
    fun recentSessions(limit: Int = 20): Flow<List<SyncSession>> = syncSessionDao.getRecentSessions(limit)
    suspend fun sessionsInRange(start: Long, end: Long): List<SyncSession> = syncSessionDao.getByDateRange(start, end)

    fun summaryByDate(date: String): Flow<DailySummary?> = dailySummaryDao.getByDate(date)
    fun lastNDays(n: Int): Flow<List<DailySummary>> = dailySummaryDao.getLastNDays(n)
    fun allSummaries(): Flow<List<DailySummary>> = dailySummaryDao.getAll()
    suspend fun upsertSummary(summary: DailySummary) = dailySummaryDao.upsert(summary)
    suspend fun updateRecommendation(date: String, text: String) = dailySummaryDao.updateRecommendation(date, text)
    suspend fun deleteSessionsOlderThan(cutoff: Long) = syncSessionDao.deleteOlderThan(cutoff)

    suspend fun clearAllData() {
        syncSessionDao.deleteAll()
        dailySummaryDao.deleteAll()
    }
}
