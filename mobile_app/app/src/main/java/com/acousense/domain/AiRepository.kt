package com.acousense.domain

import com.acousense.data.ai.GemmaInference
import com.acousense.data.ai.PromptBuilder
import com.acousense.data.db.DailySummary
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class AiRepository @Inject constructor(
    private val gemmaInference: GemmaInference,
) {
    suspend fun generateForSummary(summary: DailySummary, onToken: (String) -> Unit): Result<String> {
        val prompt = PromptBuilder.build(summary)
        return Result.success(gemmaInference.generateStreaming(prompt, onToken))
    }

    suspend fun checkModelAvailability() {
        gemmaInference.initialize()
    }

    fun isModelAvailable(): Boolean = gemmaInference.isModelAvailable
}
