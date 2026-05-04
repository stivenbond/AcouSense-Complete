package com.acousense.data.ai

import com.acousense.data.db.DailySummary

object PromptBuilder {
    fun build(summary: DailySummary): String {
        val hours = summary.totalExposureSeconds / 3600
        val minutes = (summary.totalExposureSeconds % 3600) / 60
        val classification = when (summary.dominantClassification) {
            1 -> "Low"
            2 -> "Moderate"
            3 -> "High"
            else -> "None"
        }
        return """
            You are a WHO-certified noise exposure health advisor.
            Analyze this user's noise exposure data and provide 3-4 specific, actionable health recommendations. Be concise and practical.
            Date: ${summary.date}
            Average noise level: ${summary.avgLevel} (scale 0-1023, where 700+ is harmful)
            Maximum peak: ${summary.maxLevel}
            Total exposure time: ${hours}h ${minutes}m
            Peak events above high threshold: ${summary.peakCount}
            Overall classification: $classification (None/Low/Moderate/High)
            WHO recommendation: Noise above 85dB for more than 8 hours/day causes permanent hearing damage. Equivalent to approximately 600+ on this scale.
            Provide recommendations:
        """.trimIndent()
    }
}
