package com.acousense.data.db

import android.content.Context
import android.content.SharedPreferences
import androidx.hilt.work.HiltWorker
import androidx.work.CoroutineWorker
import androidx.work.WorkerParameters
import com.acousense.domain.ExposureRepository
import dagger.assisted.Assisted
import dagger.assisted.AssistedInject
import java.time.LocalDate
import java.time.ZoneId

@HiltWorker
class DailySummaryWorker @AssistedInject constructor(
    @Assisted appContext: Context,
    @Assisted params: WorkerParameters,
    private val repository: ExposureRepository,
    private val prefs: SharedPreferences
) : CoroutineWorker(appContext, params) {

    override suspend fun doWork(): Result {
        val yesterday = LocalDate.now().minusDays(1)
        val zone = ZoneId.systemDefault()
        val start = yesterday.atStartOfDay(zone).toEpochSecond()
        val end = yesterday.plusDays(1).atStartOfDay(zone).toEpochSecond() - 1

        val sessions = repository.sessionsInRange(start, end)
        if (sessions.isNotEmpty()) {
            val avg = sessions.map { it.avgLevel }.average().toFloat()
            val max = sessions.maxOf { it.maxLevel }
            val exposure = sessions.sumOf { it.exposureSeconds }
            val peaks = sessions.sumOf { it.peakCount }
            val dominant = sessions
                .groupBy { it.classification }
                .maxByOrNull { it.value.size }
                ?.key ?: 0
            repository.upsertSummary(
                DailySummary(
                    date = yesterday.toString(),
                    avgLevel = avg,
                    maxLevel = max,
                    totalExposureSeconds = exposure,
                    peakCount = peaks,
                    dominantClassification = dominant,
                    sessionCount = sessions.size,
                    aiRecommendation = null
                )
            )
        }

        val retentionDays = prefs.getInt("retention_days", 30)
        if (retentionDays > 0) {
            val cutoff = LocalDate.now().minusDays(retentionDays.toLong()).atStartOfDay(zone).toEpochSecond()
            repository.deleteSessionsOlderThan(cutoff)
        }
        return Result.success()
    }
}
