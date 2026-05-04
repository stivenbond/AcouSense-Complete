package com.acousense.data.db

import androidx.compose.runtime.Immutable
import androidx.room.Entity
import androidx.room.PrimaryKey

@Immutable
@Entity(tableName = "daily_summaries")
data class DailySummary(
    @PrimaryKey val date: String,
    val avgLevel: Float,
    val maxLevel: Int,
    val totalExposureSeconds: Long,
    val peakCount: Int,
    val dominantClassification: Int,
    val sessionCount: Int,
    val aiRecommendation: String?
)
