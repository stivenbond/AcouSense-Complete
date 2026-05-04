package com.acousense.ui.recommendations

import androidx.compose.runtime.Immutable
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.acousense.data.db.DailySummary
import com.acousense.domain.AiRepository
import com.acousense.domain.ExposureRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import javax.inject.Inject
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

@Immutable
data class RecommendationsUiState(
    val selectedDate: String = "",
    val dailySummary: DailySummary? = null,
    val streamedText: String = "",
    val isGenerating: Boolean = false,
    val isModelAvailable: Boolean = true,
)

@HiltViewModel
class RecommendationsViewModel @Inject constructor(
    private val repository: ExposureRepository,
    private val aiRepository: AiRepository
) : ViewModel() {
    private val _state = MutableStateFlow(RecommendationsUiState())
    val state: StateFlow<RecommendationsUiState> = _state.asStateFlow()
    private var loadJob: Job? = null

    init {
        viewModelScope.launch {
            aiRepository.checkModelAvailability()
            _state.value = _state.value.copy(
                isModelAvailable = aiRepository.isModelAvailable(),
            )
        }
    }

    fun load(date: String) {
        if (date.isBlank()) return
        loadJob?.cancel()
        loadJob = viewModelScope.launch {
            repository.summaryByDate(date).collect { summary ->
                _state.value = _state.value.copy(selectedDate = date, dailySummary = summary)
            }
        }
    }

    fun generateAdvice() {
        val summary = _state.value.dailySummary ?: return
        viewModelScope.launch {
            _state.value = _state.value.copy(isGenerating = true, streamedText = "")
            val result = aiRepository.generateForSummary(summary) { token ->
                _state.value = _state.value.copy(streamedText = _state.value.streamedText + token)
            }
            result.fold(
                onSuccess = { text ->
                    repository.updateRecommendation(summary.date, text)
                },
                onFailure = {
                    _state.value = _state.value.copy(
                        streamedText = "Unable to generate advice. Please check your model configuration in Settings.",
                    )
                },
            )
            _state.value = _state.value.copy(isGenerating = false)
        }
    }
}
