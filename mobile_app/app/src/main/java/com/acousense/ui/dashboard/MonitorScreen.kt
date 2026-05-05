package com.acousense.ui.dashboard

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.acousense.ui.ExposureArcGauge
import com.acousense.ui.PulsingDot
import com.acousense.ui.SectionHeader
import com.acousense.ui.StatCard
import com.acousense.ui.SurfaceCard
import com.acousense.ui.formatExposure
import com.acousense.ui.formatLevelDb
import com.acousense.ui.theme.NoiseHigh
import com.acousense.ui.theme.NoiseLow

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MonitorScreen(viewModel: DashboardViewModel = hiltViewModel()) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val esp = state.espStatus
    val latestLevel = state.latestSession?.avgLevel ?: esp.lastReadingAvg ?: 0
    val latestClass = state.latestSession?.classification ?: esp.lastReadingAlertLevel ?: 0

    Scaffold(
        containerColor = MaterialTheme.colorScheme.background,
        contentWindowInsets = WindowInsets(0, 0, 0, 0),
        topBar = {
            TopAppBar(
                title = { Text("Monitor") },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.background,
                    titleContentColor = MaterialTheme.colorScheme.onBackground,
                ),
            )
        },
    ) { innerPadding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            SurfaceCard(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(300.dp),
            ) {
                ExposureArcGauge(
                    value = latestLevel,
                    classification = latestClass,
                    modifier = Modifier.fillMaxSize(),
                )
            }

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                StatCard(
                    label = "ESP API",
                    value = if (esp.isReachable) "ONLINE" else "OFFLINE",
                    dotColor = if (esp.isReachable) NoiseLow else NoiseHigh,
                    modifier = Modifier.weight(1f),
                )
                StatCard(
                    label = "ARDUINO",
                    value = if (esp.arduinoOnline) "ONLINE" else "OFFLINE",
                    dotColor = if (esp.arduinoOnline) NoiseLow else NoiseHigh,
                    modifier = Modifier.weight(1f),
                )
                StatCard(
                    label = "BLE",
                    value = if (state.isGattLive) "READY" else "STOPPED",
                    dotColor = if (state.isGattLive) NoiseLow else NoiseHigh,
                    modifier = Modifier.weight(1f),
                )
            }

            SurfaceCard(modifier = Modifier.fillMaxWidth()) {
                Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    SectionHeader(title = "Live Sources")
                    StatusRow("ESP32 API", if (esp.isReachable) "Reachable" else (esp.errorMessage ?: "Unavailable"))
                    StatusRow("Arduino link", if (esp.arduinoOnline) "Online" else "Offline")
                    StatusRow("SD card", if (esp.sdCardOk) "OK" else "Error")
                    StatusRow("Bluetooth scan", esp.bluetoothStatus.uppercase())
                    StatusRow("Stored readings", esp.readingCount.toString())
                    if (esp.wifiRssi != null) StatusRow("Wi-Fi RSSI", "${esp.wifiRssi} dBm")
                }
            }

            state.todaySummary?.let { summary ->
                SurfaceCard(modifier = Modifier.fillMaxWidth()) {
                    Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                        SectionHeader(title = "Today")
                        StatusRow("Average", formatLevelDb(summary.avgLevel.toInt()))
                        StatusRow("Peak", formatLevelDb(summary.maxLevel))
                        StatusRow("Exposure", formatExposure(summary.totalExposureSeconds))
                        StatusRow("Sessions", summary.sessionCount.toString())
                    }
                }
            }
        }
    }
}

@Composable
private fun StatusRow(label: String, value: String) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            PulsingDot(color = MaterialTheme.colorScheme.primary)
            Text(label, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurface)
        }
        Text(value, style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}
