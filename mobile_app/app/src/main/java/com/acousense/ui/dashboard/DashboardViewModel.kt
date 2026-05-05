package com.acousense.ui.dashboard

import androidx.compose.runtime.Immutable
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.acousense.data.api.EspApiRepository
import com.acousense.data.api.EspStatusSnapshot
import com.acousense.data.db.DailySummary
import com.acousense.data.db.SyncSession
import com.acousense.domain.BleRepository
import com.acousense.domain.ExposureRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import javax.inject.Inject
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import kotlinx.coroutines.delay

@Immutable
data class DashboardUiState(
    val latestSession: SyncSession? = null,
    val todaySummary: DailySummary? = null,
    val isGattLive: Boolean = false,
    val isBluetoothOn: Boolean = true,
    val espStatus: EspStatusSnapshot = EspStatusSnapshot(),
)

@HiltViewModel
class DashboardViewModel @Inject constructor(
    repository: ExposureRepository,
    bleRepository: BleRepository,
    private val espApiRepository: EspApiRepository,
) : ViewModel() {
    private val zoneId = ZoneId.systemDefault()
    private val today = LocalDate.now(zoneId).toString()

    val state: StateFlow<DashboardUiState> = combine(
        repository.latestSession(),
        repository.recentSessions(limit = 100),
        bleRepository.gattRunning,
        bleRepository.bluetoothEnabled,
        repository.summaryByDate(today),
        espApiRepository.status,
    ) { latestSession, recentSessions, isGattLive, isBluetoothOn, storedSummary, espStatus ->
        val startOfDay = LocalDate.parse(today).atStartOfDay(zoneId).toEpochSecond()
        val endOfDay = Instant.now().epochSecond
        val todaySessions = recentSessions.filter { session ->
            session.periodStart in startOfDay..endOfDay
        }

        DashboardUiState(
            latestSession = latestSession,
            todaySummary = when {
                todaySessions.isNotEmpty() -> buildTodaySummary(todaySessions, today)
                else -> storedSummary
            },
            isGattLive = isGattLive,
            isBluetoothOn = isBluetoothOn,
            espStatus = espStatus,
        )
    }.stateIn(
        scope = viewModelScope,
        started = SharingStarted.WhileSubscribed(5_000),
        initialValue = DashboardUiState(),
    )

    init {
        viewModelScope.launch {
            while (true) {
                espApiRepository.refresh()
                delay(15_000)
            }
        }
    }
}

private fun buildTodaySummary(sessions: List<SyncSession>, date: String): DailySummary {
    val avgLevel = sessions.map { it.avgLevel.toFloat() }.average().toFloat()
    val maxLevel = sessions.maxOf { it.maxLevel }
    val totalExposure = sessions.sumOf { it.exposureSeconds }
    val peakCount = sessions.sumOf { it.peakCount }
    val dominantClassification = sessions
        .groupBy { it.classification }
        .maxByOrNull { it.value.size }
        ?.key
        ?: 0

    return DailySummary(
        date = date,
        avgLevel = avgLevel,
        maxLevel = maxLevel,
        totalExposureSeconds = totalExposure,
        peakCount = peakCount,
        dominantClassification = dominantClassification,
        sessionCount = sessions.size,
        aiRecommendation = null,
    )
}
