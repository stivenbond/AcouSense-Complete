package com.acousense.data.ai

import android.content.Context
import android.content.SharedPreferences
import com.google.mediapipe.tasks.genai.llminference.LlmInference
import com.google.mediapipe.tasks.genai.llminference.LlmInferenceSession
import com.acousense.data.settings.SettingsRepository
import dagger.hilt.android.qualifiers.ApplicationContext
import java.io.File
import java.io.FileNotFoundException
import javax.inject.Inject
import javax.inject.Singleton
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.withContext

data class LlmGenerationConfig(
    val temperature: Float,
    val topK: Int,
) {
    companion object {
        val ADVICE = LlmGenerationConfig(
            temperature = 0.15f,
            topK = 8,
        )
    }
}

@Singleton
class GemmaInference @Inject constructor(
    @ApplicationContext private val context: Context,
    private val prefs: SharedPreferences,
    private val settingsRepository: SettingsRepository,
) {
    private var llmInference: LlmInference? = null

    val isModelAvailable: Boolean
        get() = llmInference != null

    /**
     * Initialises the MediaPipe LLM engine.
     * Call once before generating; subsequent calls are no-ops if already initialised.
     */
    suspend fun initialize(): Result<Unit> = withContext(Dispatchers.IO) {
        if (llmInference != null) return@withContext Result.success(Unit)

        val modelPath = settingsRepository.getModelPath()
        if (!File(modelPath).exists()) {
            return@withContext Result.failure(
                FileNotFoundException("Model not found at $modelPath. Set the correct path in Settings.")
            )
        }

        val settings = settingsRepository.inferenceSettings.value

        return@withContext runCatching {
            val options = LlmInference.LlmInferenceOptions.builder()
                .setModelPath(modelPath)
                .setMaxTokens(settings.maxTokens)
                .build()
            llmInference = LlmInference.createFromOptions(context, options)
        }
    }

    /**
     * Streams tokens to [onToken] as they are generated, then returns the full response.
     */
    suspend fun generateStreaming(
        prompt: String,
        onPartial: (String) -> Unit,
        generationConfig: LlmGenerationConfig? = null,
    ): String = withContext(Dispatchers.IO) {
        if (llmInference == null) {
            initialize().getOrThrow()
        }

        val engine = llmInference
            ?: throw IllegalStateException("LLM engine failed to initialise")

        // Create a new channel for this specific request
        val channel = Channel<Pair<String, Boolean>>(capacity = Channel.UNLIMITED)

        val settings = settingsRepository.inferenceSettings.value
        val temperature = generationConfig?.temperature ?: settings.temperature
        val topK = generationConfig?.topK ?: settings.topK

        // Configure session options for this generation
        val sessionOptions = LlmInferenceSession.LlmInferenceSessionOptions.builder()
            .setTemperature(temperature)
            .setTopK(topK)
            .setRandomSeed(101)
            .build()

        val session = LlmInferenceSession.createFromOptions(engine, sessionOptions)

        try {
            session.addQueryChunk(prompt)
            session.generateResponseAsync { partialResult, done ->
                channel.trySend(Pair(partialResult, done))
            }

            var assembledResponse = ""
            val fullResponse = buildString {
                while (true) {
                    val (partialText, done) = channel.receive()
                    if (partialText.isNotEmpty()) {
                        assembledResponse = when {
                            // Some runtimes stream the full partial response each callback.
                            partialText.startsWith(assembledResponse) -> partialText
                            // Others stream only deltas/tokens.
                            else -> assembledResponse + partialText
                        }
                        onPartial(assembledResponse)
                    }
                    if (done) break
                }
                append(assembledResponse)
            }
            fullResponse
        } finally {
            channel.close()
            session.close()
        }
    }

    /**
     * Release the native engine when no longer needed.
     */
    fun close() {
        llmInference?.close()
        llmInference = null
    }
}
