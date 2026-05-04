package com.acousense

import android.app.Application
import com.acousense.data.db.DailySummary
import com.acousense.data.db.DailySummaryDao
import com.acousense.data.db.SyncSession
import com.acousense.data.db.SyncSessionDao
import androidx.hilt.work.HiltWorkerFactory
import androidx.work.Configuration
import dagger.hilt.android.HiltAndroidApp
import javax.inject.Inject
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import java.time.LocalDate
import java.time.ZoneId

@HiltAndroidApp
class AcouSenseApp : Application(), Configuration.Provider {
    @Inject lateinit var workerFactory: HiltWorkerFactory
    @Inject lateinit var syncSessionDao: SyncSessionDao
    @Inject lateinit var dailySummaryDao: DailySummaryDao
    private val appScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)

    override val workManagerConfiguration: Configuration
        get() = Configuration.Builder()
            .setWorkerFactory(workerFactory)
            .build()

    override fun onCreate() {
        super.onCreate()
        appScope.launch {
            seedDemoDataIfNeeded()
        }
    }

    private suspend fun seedDemoDataIfNeeded() {
        if (syncSessionDao.countAll() > 0 || dailySummaryDao.countAll() > 0) return

        val zone = ZoneId.systemDefault()
        val today = LocalDate.now(zone)
        for (i in 0 until 7) {
            val date = today.minusDays(i.toLong())
            val dateStartSec = date.atStartOfDay(zone).toEpochSecond()
            val sessionsForDay = 2 + (i % 3)
            var totalExposure = 0L
            var maxLevel = 0
            var totalAvg = 0f
            var totalPeaks = 0

            repeat(sessionsForDay) { s ->
                val avg = (360 + (s * 70) + i * 15).coerceIn(220, 760)
                val peak = (avg + 120 + s * 25).coerceIn(350, 1023)
                val exposureSecs = (20 * 60L) + (s * 8 * 60L) + (i * 90L)
                val classification = when {
                    avg <= 400 -> 0
                    avg <= 700 -> 2
                    else -> 3
                }
                val start = dateStartSec + (9 + s * 3) * 3600L
                val end = start + exposureSecs

                syncSessionDao.insert(
                    SyncSession(
                        userIdentifier = "demo_user",
                        periodStart = start,
                        periodEnd = end,
                        avgLevel = avg,
                        maxLevel = peak,
                        peakCount = (2 + s + (i % 2)),
                        exposureSeconds = exposureSecs,
                        classification = classification,
                    )
                )

                totalExposure += exposureSecs
                maxLevel = maxOf(maxLevel, peak)
                totalAvg += avg.toFloat()
                totalPeaks += (2 + s + (i % 2))
            }

            val dailyAvg = (totalAvg / sessionsForDay).coerceIn(0f, 1023f)
            val dailyClass = when {
                dailyAvg <= 400f -> 0
                dailyAvg <= 700f -> 2
                else -> 3
            }

            dailySummaryDao.upsert(
                DailySummary(
                    date = date.toString(),
                    avgLevel = dailyAvg,
                    maxLevel = maxLevel,
                    totalExposureSeconds = totalExposure,
                    peakCount = totalPeaks,
                    dominantClassification = dailyClass,
                    sessionCount = sessionsForDay,
                    aiRecommendation = if (i == 0) {
                        "Good progress today. Keep breaks between noisy blocks and use hearing protection in peak moments."
                    } else {
                        null
                    },
                )
            )
        }
    }
}
