package com.acousense.ui.history

import androidx.compose.runtime.Immutable
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.acousense.data.db.DailySummary
import com.acousense.domain.ExposureRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import java.time.LocalDate
import javax.inject.Inject
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn

@Immutable
data class HistoryUiState(
    val range: String = "7D",
    val summaries: List<DailySummary> = emptyList(),
)

@HiltViewModel
class HistoryViewModel @Inject constructor(
    repository: ExposureRepository,
) : ViewModel() {
    private val selectedRange = MutableStateFlow("7D")

    val state: StateFlow<HistoryUiState> = combine(
        selectedRange,
        repository.allSummaries(),
    ) { range, allSummaries ->
        val filteredSummaries = when (range) {
            "7D" -> filterByDays(allSummaries, days = 7)
            "30D" -> filterByDays(allSummaries, days = 30)
            else -> allSummaries
        }.sortedByDescending { it.date }

        HistoryUiState(
            range = range,
            summaries = filteredSummaries,
        )
    }.stateIn(
        scope = viewModelScope,
        started = SharingStarted.WhileSubscribed(5_000),
        initialValue = HistoryUiState(),
    )

    fun setRange(range: String) {
        selectedRange.value = range
    }
}

private fun filterByDays(summaries: List<DailySummary>, days: Long): List<DailySummary> {
    val cutoff = LocalDate.now().minusDays(days - 1)
    return summaries.filter { summary ->
        runCatching { LocalDate.parse(summary.date) }
            .getOrNull()
            ?.let { !it.isBefore(cutoff) }
            ?: false
    }
}
