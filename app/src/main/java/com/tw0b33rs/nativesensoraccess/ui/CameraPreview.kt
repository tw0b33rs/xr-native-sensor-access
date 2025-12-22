@file:Suppress("ComposeUnstableReceiver", "ComposeModifierMissing", "ComposeCompositionLocalUsage")

package com.tw0b33rs.nativesensoraccess.ui

import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Card
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.MenuAnchorType
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import com.tw0b33rs.nativesensoraccess.sensor.CameraClusterState
import com.tw0b33rs.nativesensoraccess.sensor.CameraClusterType

/**
 * Camera preview surface using AndroidView with SurfaceView for 2D mode.
 * Provides zero-copy rendering from native camera stream.
 */
@Composable
fun CameraPreviewSurface(
    cameraId: String?,
    onSurfaceReady: (android.view.Surface) -> Unit,
    onSurfaceDestroyed: () -> Unit,
    modifier: Modifier = Modifier
) {
    var surfaceReady by remember { mutableStateOf(false) }
    // Track the last camera ID we started preview for to prevent duplicate calls
    var lastStartedCameraId by remember { mutableStateOf<String?>(null) }

    //noinspection ComposeUnstableReceiver - Box composable invocation is valid here
    Box(
        modifier = modifier
            .background(Color.Black)
    ) {
        AndroidView(
            factory = { context ->
                SurfaceView(context).apply {
                    holder.addCallback(object : SurfaceHolder.Callback {
                        override fun surfaceCreated(holder: SurfaceHolder) {
                            surfaceReady = true
                            if (cameraId != null && lastStartedCameraId != cameraId) {
                                lastStartedCameraId = cameraId
                                onSurfaceReady(holder.surface)
                            }
                        }

                        override fun surfaceChanged(
                            holder: SurfaceHolder,
                            format: Int,
                            width: Int,
                            height: Int
                        ) {
                            // Surface size changed - could restart preview if needed
                        }

                        override fun surfaceDestroyed(holder: SurfaceHolder) {
                            surfaceReady = false
                            lastStartedCameraId = null
                            onSurfaceDestroyed()
                        }
                    })
                }
            },
            update = { surfaceView ->
                // Only restart preview when cameraId actually CHANGES (not on every recomposition)
                if (surfaceReady && cameraId != null && cameraId != lastStartedCameraId) {
                    lastStartedCameraId = cameraId
                    onSurfaceReady(surfaceView.holder.surface)
                }
            },
            modifier = Modifier.fillMaxSize()
        )

        // Overlay when no camera selected
        if (cameraId == null) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.7f)),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = "No camera selected",
                    color = Color.White,
                    style = MaterialTheme.typography.bodyLarge
                )
            }
        }
    }
}

/**
 * Camera cluster view with dropdown selection and preview.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun CameraClusterView(
    clusterState: CameraClusterState,
    clusterType: CameraClusterType,
    clusterTitle: String,
    onCameraSelected: (String) -> Unit,
    onSurfaceReady: (String, android.view.Surface) -> Unit,
    onSurfaceDestroyed: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    Column(modifier = modifier.fillMaxSize()) {
        // Header with camera count
        Text(
            text = "$clusterTitle (${clusterState.cameras.size} cameras)",
            style = MaterialTheme.typography.headlineSmall,
            modifier = Modifier.padding(bottom = 16.dp)
        )

        if (clusterState.cameras.isEmpty()) {
            // No cameras in this cluster
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(16f / 9f)
                    .background(MaterialTheme.colorScheme.surfaceVariant),
                contentAlignment = Alignment.Center
            ) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Text(
                        text = "No cameras available",
                        style = MaterialTheme.typography.bodyLarge
                    )
                    Text(
                        text = "This cluster has no detected cameras",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
        } else if (clusterType == CameraClusterType.PASSTHROUGH && clusterState.cameras.size >= 2) {
            // Special layout for side-by-side passthrough cameras (square 1:1 ratio for 3000x3000)
            Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                clusterState.cameras.take(2).forEach { camera ->
                    Column(modifier = Modifier.weight(1f)) {
                        Text(camera.displayName, style = MaterialTheme.typography.bodyLarge)
                        Card(modifier = Modifier.aspectRatio(1f)) {
                            CameraPreviewSurface(
                                cameraId = camera.id,
                                onSurfaceReady = { surface -> onSurfaceReady(camera.id, surface) },
                                onSurfaceDestroyed = { onSurfaceDestroyed(camera.id) }
                            )
                        }
                        Spacer(modifier = Modifier.height(4.dp))
                        // Stats per camera
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceEvenly
                        ) {
                            Text(
                                text = "%.1f Hz".format(clusterState.stats.frameRateHz),
                                style = MaterialTheme.typography.labelMedium
                            )
                            Text(
                                text = "%.1f ms".format(clusterState.stats.latencyMs),
                                style = MaterialTheme.typography.labelMedium
                            )
                        }
                    }
                }
            }
        } else {
            // Camera dropdown
            var expanded by remember { mutableStateOf(false) }
            val selectedCamera = clusterState.cameras.find { it.id == clusterState.selectedCameraId }

            ExposedDropdownMenuBox(
                expanded = expanded,
                onExpandedChange = { expanded = it },
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 16.dp)
            ) {
                OutlinedTextField(
                    value = selectedCamera?.displayName ?: "Select Camera",
                    onValueChange = {},
                    readOnly = true,
                    label = { Text("Camera") },
                    trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
                    colors = ExposedDropdownMenuDefaults.outlinedTextFieldColors(),
                    modifier = Modifier
                        .menuAnchor(MenuAnchorType.PrimaryNotEditable)
                        .fillMaxWidth()
                )

                ExposedDropdownMenu(
                    expanded = expanded,
                    onDismissRequest = { expanded = false }
                ) {
                    clusterState.cameras.forEach { camera ->
                        DropdownMenuItem(
                            text = {
                                Column {
                                    Text(
                                        text = camera.displayName,
                                        style = MaterialTheme.typography.bodyLarge
                                    )
                                    Text(
                                        text = "${camera.resolution} @ ${camera.maxFps} fps",
                                        style = MaterialTheme.typography.labelSmall
                                    )
                                }
                            },
                            onClick = {
                                onCameraSelected(camera.id)
                                expanded = false
                            }
                        )
                    }
                }
            }

            // Camera preview
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
            ) {
                CameraPreviewSurface(
                    cameraId = clusterState.selectedCameraId,
                    onSurfaceReady = { surface ->
                        clusterState.selectedCameraId?.let { cameraId ->
                            onSurfaceReady(cameraId, surface)
                        }
                    },
                    onSurfaceDestroyed = {
                        clusterState.selectedCameraId?.let { cameraId ->
                            onSurfaceDestroyed(cameraId)
                        }
                    },
                    modifier = Modifier.fillMaxSize()
                )
            }

            // Stats row
            if (clusterState.isStreaming) {
                Spacer(modifier = Modifier.height(8.dp))
                CameraStatsRow(
                    frameRateHz = clusterState.stats.frameRateHz,
                    latencyMs = clusterState.stats.latencyMs,
                    frameCount = clusterState.stats.frameCount
                )
            }
        }
    }
}

/**
 * Camera statistics row.
 */
@Composable
fun CameraStatsRow(
    frameRateHz: Float,
    latencyMs: Float,
    frameCount: Long,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceEvenly
    ) {
        StatItem(label = "FPS", value = "%.1f".format(frameRateHz))
        StatItem(label = "Latency", value = "%.1f ms".format(latencyMs))
        StatItem(label = "Frames", value = frameCount.toString())
    }
}

@Composable
private fun StatItem(label: String, value: String) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(
            text = value,
            style = MaterialTheme.typography.titleMedium
        )
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
    }
}

/**
 * Avatar camera view - no dropdown, auto-selects first available camera.
 * No latency or frame count shown.
 */
@Composable
fun AvatarClusterView(
    clusterState: CameraClusterState,
    onSurfaceReady: (String, android.view.Surface) -> Unit,
    onSurfaceDestroyed: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    Column(modifier = modifier.fillMaxSize()) {
        Text(
            text = "Avatar",
            style = MaterialTheme.typography.headlineSmall,
            modifier = Modifier.padding(bottom = 16.dp)
        )

        if (clusterState.cameras.isEmpty()) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(16f / 9f)
                    .background(MaterialTheme.colorScheme.surfaceVariant),
                contentAlignment = Alignment.Center
            ) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Text(
                        text = "No webcam available",
                        style = MaterialTheme.typography.bodyLarge
                    )
                    Text(
                        text = "Avatar camera not detected",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
        } else {
            // Auto-select first camera (webcam)
            val camera = clusterState.cameras.first()

            // Calculate aspect ratio from camera resolution
            val aspectRatio = if (camera.width > 0 && camera.height > 0) {
                camera.width.toFloat() / camera.height.toFloat()
            } else {
                16f / 9f // Default fallback
            }

            Text(
                text = camera.displayName,
                style = MaterialTheme.typography.bodyLarge,
                modifier = Modifier.padding(bottom = 8.dp)
            )

            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(aspectRatio)
            ) {
                CameraPreviewSurface(
                    cameraId = camera.id,
                    onSurfaceReady = { surface -> onSurfaceReady(camera.id, surface) },
                    onSurfaceDestroyed = { onSurfaceDestroyed(camera.id) },
                    modifier = Modifier.fillMaxSize()
                )
            }
        }
    }
}

/**
 * Eye tracking placeholder - cameras not accessible on Android XR.
 * Shows empty view for future eye tracking data output.
 */
@Composable
fun EyeTrackingPlaceholder(
    modifier: Modifier = Modifier
) {
    Column(modifier = modifier.fillMaxSize()) {
        Text(
            text = "Eye Tracking",
            style = MaterialTheme.typography.headlineSmall,
            modifier = Modifier.padding(bottom = 16.dp)
        )

        Box(
            modifier = Modifier
                .fillMaxWidth()
                .weight(1f)
                .background(MaterialTheme.colorScheme.surfaceVariant),
            contentAlignment = Alignment.Center
        ) {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text(
                    text = "👀",
                    style = MaterialTheme.typography.displayLarge
                )
                Spacer(modifier = Modifier.height(16.dp))
                Text(
                    text = "Eye Tracking Data",
                    style = MaterialTheme.typography.headlineSmall
                )
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    text = "Eye tracking camera access is restricted on Android XR.\nEye tracking output data will be displayed here.",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(horizontal = 32.dp)
                )
            }
        }
    }
}

/**
 * Permission request UI.
 */
@Composable
fun CameraPermissionRequest(
    onRequestPermission: () -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier.fillMaxSize(),
        contentAlignment = Alignment.Center
    ) {
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Text(
                text = "📷",
                style = MaterialTheme.typography.displayLarge
            )
            Text(
                text = "Camera Permission Required",
                style = MaterialTheme.typography.headlineSmall
            )
            Text(
                text = "This app needs camera access to display XR sensor streams.",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            androidx.compose.material3.Button(onClick = onRequestPermission) {
                Text("Grant Permission")
            }
        }
    }
}
