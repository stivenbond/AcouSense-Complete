package com.acousense

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.BarChart
import androidx.compose.material.icons.filled.Hearing
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.outlined.AutoAwesome
import androidx.compose.material.icons.outlined.BarChart
import androidx.compose.material.icons.outlined.Hearing
import androidx.compose.material.icons.outlined.Settings
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import androidx.core.view.WindowCompat
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import com.acousense.data.ble.BleGattService
import com.acousense.data.db.DailySummaryWorker
import com.acousense.ui.dashboard.MonitorScreen
import com.acousense.ui.history.HistoryScreen
import com.acousense.ui.recommendations.RecommendationsScreen
import com.acousense.ui.settings.SettingsScreen
import com.acousense.ui.theme.AcouSenseBackground
import com.acousense.ui.theme.AcouSenseTheme
import dagger.hilt.android.AndroidEntryPoint
import java.util.concurrent.TimeUnit

@AndroidEntryPoint
class MainActivity : ComponentActivity() {
    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions(),
    ) { permissions ->
        val allGranted = permissions.entries.all { it.value }
        if (allGranted) {
            startBleService()
        }
    }

    private fun checkAndRequestPermissions() {
        val permissionsToRequest = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissionsToRequest += Manifest.permission.BLUETOOTH_SCAN
            permissionsToRequest += Manifest.permission.BLUETOOTH_CONNECT
            permissionsToRequest += Manifest.permission.BLUETOOTH_ADVERTISE
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            permissionsToRequest += Manifest.permission.POST_NOTIFICATIONS
        }

        val missingPermissions = permissionsToRequest.filter { permission ->
            ContextCompat.checkSelfPermission(this, permission) != PackageManager.PERMISSION_GRANTED
        }

        if (missingPermissions.isNotEmpty()) {
            permissionLauncher.launch(missingPermissions.toTypedArray())
        } else {
            startBleService()
        }
    }

    private fun startBleService() {
        runCatching {
            ContextCompat.startForegroundService(this, Intent(this, BleGattService::class.java))
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val systemBarColor = AcouSenseBackground.toArgb()
        enableEdgeToEdge(
            statusBarStyle = SystemBarStyle.dark(systemBarColor),
            navigationBarStyle = SystemBarStyle.dark(systemBarColor),
        )
        WindowCompat.setDecorFitsSystemWindows(window, false)

        checkAndRequestPermissions()

        val workRequest = PeriodicWorkRequestBuilder<DailySummaryWorker>(1, TimeUnit.DAYS).build()
        WorkManager.getInstance(this).enqueueUniquePeriodicWork(
            "daily_summary_worker",
            ExistingPeriodicWorkPolicy.UPDATE,
            workRequest,
        )

        setContent {
            AcouSenseTheme {
                var currentRoute by rememberSaveable { mutableStateOf("monitor") }
                var selectedAdviceDate by rememberSaveable { mutableStateOf<String?>(null) }

                val navigationItems = listOf(
                    NavigationItem("monitor", "Monitor", Icons.Outlined.Hearing, Icons.Filled.Hearing),
                    NavigationItem("history", "History", Icons.Outlined.BarChart, Icons.Filled.BarChart),
                    NavigationItem("ai_advice", "AI Advice", Icons.Outlined.AutoAwesome, Icons.Filled.AutoAwesome),
                    NavigationItem("settings", "Settings", Icons.Outlined.Settings, Icons.Filled.Settings),
                )

                Scaffold(
                    containerColor = MaterialTheme.colorScheme.background,
                    bottomBar = {
                        NavigationBar(
                            containerColor = MaterialTheme.colorScheme.surfaceContainer,
                            tonalElevation = 0.dp,
                        ) {
                            navigationItems.forEach { item ->
                                val isSelected = currentRoute == item.route
                                NavigationBarItem(
                                    selected = isSelected,
                                    onClick = {
                                        currentRoute = item.route
                                        if (item.route == "ai_advice") {
                                            selectedAdviceDate = null
                                        }
                                    },
                                    icon = {
                                        Icon(
                                            imageVector = if (isSelected) item.selectedIcon else item.icon,
                                            contentDescription = item.label,
                                        )
                                    },
                                    label = {
                                        Text(
                                            text = item.label,
                                            style = MaterialTheme.typography.labelSmall,
                                        )
                                    },
                                    colors = NavigationBarItemDefaults.colors(
                                        selectedIconColor = MaterialTheme.colorScheme.primary,
                                        selectedTextColor = MaterialTheme.colorScheme.primary,
                                        indicatorColor = MaterialTheme.colorScheme.primaryContainer,
                                        unselectedIconColor = MaterialTheme.colorScheme.onSurfaceVariant,
                                        unselectedTextColor = MaterialTheme.colorScheme.onSurfaceVariant,
                                    ),
                                )
                            }
                        }
                    },
                ) { innerPadding ->
                    AnimatedContent(
                        targetState = currentRoute,
                        transitionSpec = {
                            (slideInHorizontally { it / 4 } + fadeIn()) togetherWith
                                (slideOutHorizontally { -it / 4 } + fadeOut())
                        },
                        modifier = Modifier
                            .fillMaxSize()
                            .padding(innerPadding),
                        label = "main_navigation",
                    ) { route ->
                        when (route) {
                            "monitor" -> MonitorScreen()
                            "history" -> HistoryScreen(
                                onViewAdvice = { date ->
                                    selectedAdviceDate = date
                                    currentRoute = "ai_advice"
                                },
                            )
                            "ai_advice" -> RecommendationsScreen(
                                selectedDate = selectedAdviceDate ?: "",
                                onOpenSettings = { currentRoute = "settings" },
                            )
                            "settings" -> SettingsScreen()
                            else -> MonitorScreen()
                        }
                    }
                }
            }
        }
    }
}

private data class NavigationItem(
    val route: String,
    val label: String,
    val icon: ImageVector,
    val selectedIcon: ImageVector,
)
