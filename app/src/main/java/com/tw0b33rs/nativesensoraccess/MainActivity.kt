@file:Suppress("ComposeUnstableReceiver", "ComposeCompositionLocalUsage")

package com.tw0b33rs.nativesensoraccess

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CornerSize
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.FilledTonalIconButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.MenuAnchorType
import androidx.compose.material3.NavigationDrawerItem
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.PermanentDrawerSheet
import androidx.compose.material3.PermanentNavigationDrawer
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalInspectionMode
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.xr.compose.platform.LocalSession
import androidx.xr.compose.platform.LocalSpatialCapabilities
import androidx.xr.compose.platform.LocalSpatialConfiguration
import androidx.xr.compose.spatial.ContentEdge
import androidx.xr.compose.spatial.Orbiter
import androidx.xr.compose.spatial.Subspace
import androidx.xr.compose.subspace.SpatialPanel
import androidx.xr.compose.subspace.SubspaceComposable
import androidx.xr.compose.subspace.layout.SpatialRoundedCornerShape
import androidx.xr.compose.subspace.layout.SubspaceModifier
import androidx.xr.compose.subspace.layout.height
import androidx.xr.compose.subspace.layout.movable
import androidx.xr.compose.subspace.layout.resizable
import androidx.xr.compose.subspace.layout.width
import com.tw0b33rs.nativesensoraccess.sensor.CameraClusterType
import com.tw0b33rs.nativesensoraccess.sensor.ImuMetadata
import com.tw0b33rs.nativesensoraccess.sensor.ImuSample
import com.tw0b33rs.nativesensoraccess.sensor.ImuStats
import com.tw0b33rs.nativesensoraccess.sensor.NavigationDestination
import com.tw0b33rs.nativesensoraccess.sensor.SensorInfo
import com.tw0b33rs.nativesensoraccess.sensor.SensorUiState
import com.tw0b33rs.nativesensoraccess.sensor.SensorViewModel
import com.tw0b33rs.nativesensoraccess.ui.CameraClusterView
import com.tw0b33rs.nativesensoraccess.ui.CameraPermissionRequest
import com.tw0b33rs.nativesensoraccess.ui.theme.NativeSensorAccessTheme

class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        setContent {
            NativeSensorAccessTheme {
                val viewModel: SensorViewModel = viewModel()
                val uiState by viewModel.uiState.collectAsState()
                val context = LocalContext.current

                // Permission launcher
                val permissionLauncher = rememberLauncherForActivityResult(
                    contract = ActivityResultContracts.RequestPermission()
                ) { isGranted ->
                    viewModel.setCameraPermission(isGranted)
                }

                // Check initial permission state
                LaunchedEffect(Unit) {
                    val hasPermission = ContextCompat.checkSelfPermission(
                        context,
                        Manifest.permission.CAMERA
                    ) == PackageManager.PERMISSION_GRANTED
                    viewModel.setCameraPermission(hasPermission)
                }

                // Start sensors when app launches
                LaunchedEffect(Unit) {
                    viewModel.startSensors()
                }

                // Cleanup on dispose
                DisposableEffect(Unit) {
                    onDispose {
                        viewModel.stopSensors()
                    }
                }

                val spatialConfiguration = LocalSpatialConfiguration.current
                if (LocalSpatialCapabilities.current.isSpatialUiEnabled) {
                    Subspace {
                        MySpatialContent(
                            uiState = uiState,
                            viewModel = viewModel,
                            onRequestPermission = {
                                permissionLauncher.launch(Manifest.permission.CAMERA)
                            },
                            onRequestHomeSpaceMode = spatialConfiguration::requestHomeSpaceMode
                        )
                    }
                } else {
                    My2DContent(
                        uiState = uiState,
                        viewModel = viewModel,
                        onRequestPermission = {
                            permissionLauncher.launch(Manifest.permission.CAMERA)
                        },
                        onRequestFullSpaceMode = spatialConfiguration::requestFullSpaceMode
                    )
                }
            }
        }
    }
}

@Composable
@SubspaceComposable
fun MySpatialContent(
    uiState: SensorUiState,
    viewModel: SensorViewModel,
    onRequestPermission: () -> Unit,
    onRequestHomeSpaceMode: () -> Unit
) {
    //noinspection ComposeUnstableReceiver - SpatialPanel legitimately hosts UiComposables
    SpatialPanel(SubspaceModifier.width(1200.dp).height(800.dp).resizable().movable()) {
        Surface {
            AppWithNavigation(
                uiState = uiState,
                viewModel = viewModel,
                onRequestPermission = onRequestPermission,
                modifier = Modifier.fillMaxSize()
            )
        }
        Orbiter(
            position = ContentEdge.Top,
            offset = 20.dp,
            alignment = Alignment.End,
            shape = SpatialRoundedCornerShape(CornerSize(28.dp))
        ) {
            HomeSpaceModeIconButton(
                onClick = onRequestHomeSpaceMode,
                modifier = Modifier.size(56.dp)
            )
        }
    }
}

@Composable
fun My2DContent(
    uiState: SensorUiState,
    viewModel: SensorViewModel,
    onRequestPermission: () -> Unit,
    onRequestFullSpaceMode: () -> Unit
) {
    Surface {
        Row(
            modifier = Modifier.fillMaxSize(),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            AppWithNavigation(
                uiState = uiState,
                viewModel = viewModel,
                onRequestPermission = onRequestPermission,
                modifier = Modifier.weight(1f)
            )
            // Preview does not current support XR sessions.
            if (!LocalInspectionMode.current && LocalSession.current != null) {
                FullSpaceModeIconButton(
                    onClick = onRequestFullSpaceMode,
                    modifier = Modifier.padding(32.dp)
                )
            }
        }
    }
}

/**
 * Main app layout with permanent navigation drawer sidebar.
 */
@Composable
fun AppWithNavigation(
    uiState: SensorUiState,
    viewModel: SensorViewModel,
    onRequestPermission: () -> Unit,
    modifier: Modifier = Modifier
) {
    PermanentNavigationDrawer(
        drawerContent = {
            NavigationSidebar(
                currentDestination = uiState.currentDestination,
                uiState = uiState,
                onNavigate = viewModel::navigateTo
            )
        },
        modifier = modifier
    ) {
        // Content area
        Surface(
            modifier = Modifier.fillMaxSize(),
            color = MaterialTheme.colorScheme.background
        ) {
            when (uiState.currentDestination) {
                NavigationDestination.ImuSensors -> {
                    ImuContent(
                        uiState = uiState,
                        onAccelSelected = viewModel::selectAccelerometer,
                        onGyroSelected = viewModel::selectGyroscope,
                        modifier = Modifier.padding(24.dp)
                    )
                }

                NavigationDestination.PassthroughCameras -> {
                    CameraContent(
                        clusterState = uiState.passthroughCluster,
                        clusterType = CameraClusterType.PASSTHROUGH,
                        clusterTitle = "Passthrough Cameras",
                        hasCameraPermission = uiState.hasCameraPermission,
                        onRequestPermission = onRequestPermission,
                        onCameraSelected = { viewModel.selectCamera(CameraClusterType.PASSTHROUGH, it) },
                        onStartPreview = viewModel::startCameraPreview,
                        onStopPreview = { viewModel.stopCameraPreview(it) },
                        modifier = Modifier.padding(24.dp)
                    )
                }

                NavigationDestination.Avatar -> {
                    AvatarContent(
                        clusterState = uiState.trackingCluster,
                        hasCameraPermission = uiState.hasCameraPermission,
                        onRequestPermission = onRequestPermission,
                        onStartPreview = viewModel::startCameraPreview,
                        onStopPreview = { viewModel.stopCameraPreview(it) },
                        modifier = Modifier.padding(24.dp)
                    )
                }

                NavigationDestination.EyeTrackingCameras -> {
                    EyeTrackingContent(
                        modifier = Modifier.padding(24.dp)
                    )
                }
            }
        }
    }
}

/**
 * Navigation sidebar with sensor cluster groups.
 */
@Composable
fun NavigationSidebar(
    currentDestination: NavigationDestination,
    uiState: SensorUiState,
    onNavigate: (NavigationDestination) -> Unit,
    modifier: Modifier = Modifier
) {
    PermanentDrawerSheet(
        modifier = modifier.width(240.dp)
    ) {
        Column(
            modifier = Modifier
                .fillMaxHeight()
                .padding(16.dp)
        ) {
            Text(
                text = "XR Sensors",
                style = MaterialTheme.typography.headlineSmall,
                modifier = Modifier.padding(bottom = 16.dp, start = 12.dp)
            )

            HorizontalDivider(modifier = Modifier.padding(bottom = 16.dp))

            NavigationDestination.primaryItems.forEach { destination ->
                val sensorCount = when (destination) {
                    NavigationDestination.ImuSensors -> {
                        uiState.availableAccelerometers.size + uiState.availableGyroscopes.size
                    }
                    NavigationDestination.PassthroughCameras -> uiState.passthroughCluster.cameras.size
                    NavigationDestination.Avatar -> uiState.trackingCluster.cameras.size
                    NavigationDestination.EyeTrackingCameras -> uiState.eyeTrackingCluster.cameras.size
                }

                val showBadge = when (destination) {
                    NavigationDestination.ImuSensors -> {
                        uiState.availableAccelerometers.isNotEmpty() || uiState.availableGyroscopes.isNotEmpty()
                    }
                    NavigationDestination.PassthroughCameras,
                    NavigationDestination.Avatar,
                    NavigationDestination.EyeTrackingCameras -> true
                }

                NavigationItem(
                    destination = destination,
                    isSelected = currentDestination == destination,
                    sensorCount = sensorCount,
                    showBadge = showBadge,
                    onNavigate = onNavigate
                )
            }

            Spacer(modifier = Modifier.weight(1f))
        }
    }
}

@Composable
private fun NavigationItem(
    destination: NavigationDestination,
    isSelected: Boolean,
    sensorCount: Int,
    showBadge: Boolean,
    onNavigate: (NavigationDestination) -> Unit
) {
    NavigationDrawerItem(
        icon = {
            Text(
                text = destination.icon,
                style = MaterialTheme.typography.titleMedium
            )
        },
        label = {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = destination.title,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.weight(1f)
                )
                if (showBadge && sensorCount > 0) {
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(12.dp))
                            .background(MaterialTheme.colorScheme.secondaryContainer)
                            .padding(horizontal = 8.dp, vertical = 2.dp)
                    ) {
                        Text(
                            text = sensorCount.toString(),
                            style = MaterialTheme.typography.labelSmall,
                            color = MaterialTheme.colorScheme.onSecondaryContainer
                        )
                    }
                }
            }
        },
        selected = isSelected,
        onClick = { onNavigate(destination) },
        modifier = Modifier.padding(vertical = 2.dp)
    )
}

/**
 * Avatar content with permission check - no dropdown, auto-selects webcam.
 */
@Composable
fun AvatarContent(
    clusterState: com.tw0b33rs.nativesensoraccess.sensor.CameraClusterState,
    hasCameraPermission: Boolean,
    onRequestPermission: () -> Unit,
    onStartPreview: (String, android.view.Surface) -> Unit,
    onStopPreview: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    if (!hasCameraPermission) {
        CameraPermissionRequest(
            onRequestPermission = onRequestPermission,
            modifier = modifier
        )
    } else {
        com.tw0b33rs.nativesensoraccess.ui.AvatarClusterView(
            clusterState = clusterState,
            onSurfaceReady = onStartPreview,
            onSurfaceDestroyed = onStopPreview,
            modifier = modifier
        )
    }
}

/**
 * Eye tracking content - shows placeholder since cameras are not accessible.
 */
@Composable
fun EyeTrackingContent(
    modifier: Modifier = Modifier
) {
    com.tw0b33rs.nativesensoraccess.ui.EyeTrackingPlaceholder(
        modifier = modifier
    )
}

/**
 * Camera content with permission check.
 */
@Composable
fun CameraContent(
    clusterState: com.tw0b33rs.nativesensoraccess.sensor.CameraClusterState,
    clusterType: CameraClusterType,
    clusterTitle: String,
    hasCameraPermission: Boolean,
    onRequestPermission: () -> Unit,
    onCameraSelected: (String) -> Unit,
    onStartPreview: (String, android.view.Surface) -> Unit,
    onStopPreview: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    if (!hasCameraPermission) {
        CameraPermissionRequest(
            onRequestPermission = onRequestPermission,
            modifier = modifier
        )
    } else {
        CameraClusterView(
            clusterState = clusterState,
            clusterType = clusterType,
            clusterTitle = clusterTitle,
            onCameraSelected = onCameraSelected,
            onSurfaceReady = onStartPreview,
            onSurfaceDestroyed = onStopPreview,
            modifier = modifier
        )
    }
}

/**
 * IMU sensors content (renamed from MainContent).
 */
@Composable
fun ImuContent(
    uiState: SensorUiState,
    onAccelSelected: (Int) -> Unit,
    onGyroSelected: (Int) -> Unit,
    modifier: Modifier = Modifier
) {
    Column(modifier = modifier) {
        Text(
            text = "IMU Sensors",
            style = MaterialTheme.typography.headlineMedium,
            modifier = Modifier.padding(bottom = 8.dp)
        )

        Text(
            text = if (uiState.isImuRunning) "● Sensors Active" else "○ Sensors Stopped",
            style = MaterialTheme.typography.bodyMedium,
            color = if (uiState.isImuRunning) MaterialTheme.colorScheme.primary
                    else MaterialTheme.colorScheme.error,
            modifier = Modifier.padding(bottom = 24.dp)
        )

        // Sensor Selection Dropdowns
        Row(
            horizontalArrangement = Arrangement.spacedBy(16.dp),
            modifier = Modifier.padding(bottom = 24.dp)
        ) {
            SensorDropdown(
                label = "Accelerometer",
                sensors = uiState.availableAccelerometers,
                selectedHandle = uiState.selectedAccelHandle,
                onSensorSelected = onAccelSelected,
                modifier = Modifier.width(350.dp)
            )

            SensorDropdown(
                label = "Gyroscope",
                sensors = uiState.availableGyroscopes,
                selectedHandle = uiState.selectedGyroHandle,
                onSensorSelected = onGyroSelected,
                modifier = Modifier.width(350.dp)
            )
        }

        // Sensor Data Cards
        Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
            SensorCard(
                title = "Accelerometer",
                unit = "m/s²",
                sample = uiState.accelSample,
                frequencyHz = uiState.stats.accelFrequencyHz,
                latencyMs = uiState.stats.accelLatencyMs,
                minDelayUs = uiState.metadata.accelMinDelayUs,
                fifoReserved = uiState.metadata.accelFifoReserved,
                precision = 2,
                convertToDegrees = false
            )
            SensorCard(
                title = "Gyroscope",
                unit = "deg/s",
                sample = uiState.gyroSample,
                frequencyHz = uiState.stats.gyroFrequencyHz,
                latencyMs = uiState.stats.gyroLatencyMs,
                minDelayUs = uiState.metadata.gyroMinDelayUs,
                fifoReserved = uiState.metadata.gyroFifoReserved,
                precision = 1,
                convertToDegrees = true
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SensorDropdown(
    label: String,
    sensors: List<SensorInfo>,
    selectedHandle: Int,
    onSensorSelected: (Int) -> Unit,
    modifier: Modifier = Modifier
) {
    //noinspection ASSIGNED_BUT_NEVER_ACCESSED_VARIABLE - expanded is read by Compose delegate
    var expanded by remember { mutableStateOf(false) }

    ExposedDropdownMenuBox(
        expanded = expanded,
        onExpandedChange = { expanded = it },
        modifier = modifier
    ) {
        OutlinedTextField(
            value = sensors.find { it.handle == selectedHandle }?.name ?: "No Sensor",
            onValueChange = {},
            readOnly = true,
            label = { Text(label) },
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
            colors = ExposedDropdownMenuDefaults.outlinedTextFieldColors(),
            modifier = Modifier.menuAnchor(MenuAnchorType.PrimaryNotEditable).fillMaxWidth()
        )
        ExposedDropdownMenu(
            expanded = expanded,
            onDismissRequest = { expanded = false }
        ) {
            sensors.forEach { sensor ->
                DropdownMenuItem(
                    text = {
                        Column {
                            Text(text = sensor.name, style = MaterialTheme.typography.bodyLarge)
                            Text(
                                text = "${sensor.vendor} • Max ${sensor.maxFrequencyHz.toInt()} Hz" +
                                       if (sensor.isUncalibrated) " (uncal)" else "",
                                style = MaterialTheme.typography.labelSmall
                            )
                        }
                    },
                    onClick = {
                        onSensorSelected(sensor.handle)
                        expanded = false
                    }
                )
            }
        }
    }
}

@Composable
fun SensorCard(
    title: String,
    unit: String,
    sample: ImuSample,
    frequencyHz: Float,
    latencyMs: Float,
    minDelayUs: Int,
    fifoReserved: Int,
    precision: Int = 2,
    convertToDegrees: Boolean = false
) {
    val radToDeg = 180f / Math.PI.toFloat()
    val formatPattern = "%.${precision}f"

    fun formatValue(value: Float): String {
        val displayValue = if (convertToDegrees) value * radToDeg else value
        return formatPattern.format(displayValue)
    }

    Card(modifier = Modifier.width(350.dp)) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(
                text = title,
                style = MaterialTheme.typography.titleMedium,
                modifier = Modifier.padding(bottom = 8.dp)
            )

            // Raw sensor values
            Text(text = "X: ${formatValue(sample.x)} $unit")
            Text(text = "Y: ${formatValue(sample.y)} $unit")
            Text(text = "Z: ${formatValue(sample.z)} $unit")

            Spacer(modifier = Modifier.height(16.dp))

            // Performance stats
            Text(
                text = "Performance",
                style = MaterialTheme.typography.labelLarge,
                modifier = Modifier.padding(bottom = 4.dp)
            )
            Text(
                text = "Frequency: ${"%.1f".format(frequencyHz)} Hz",
                style = MaterialTheme.typography.bodySmall
            )
            Text(
                text = "Latency: ${"%.1f".format(latencyMs)} ms",
                style = MaterialTheme.typography.bodySmall
            )

            Spacer(modifier = Modifier.height(12.dp))

            // Hardware capabilities
            Text(
                text = "Hardware",
                style = MaterialTheme.typography.labelLarge,
                modifier = Modifier.padding(bottom = 4.dp)
            )
            val maxHwFreq = if (minDelayUs > 0) 1_000_000 / minDelayUs else 0
            Text(
                text = "Max Rate: $maxHwFreq Hz (min delay: $minDelayUs μs)",
                style = MaterialTheme.typography.bodySmall
            )
            Text(
                text = "FIFO: $fifoReserved events",
                style = MaterialTheme.typography.bodySmall
            )
        }
    }
}

@Composable
fun FullSpaceModeIconButton(onClick: () -> Unit, modifier: Modifier = Modifier) {
    IconButton(onClick = onClick, modifier = modifier) {
        Icon(
            painter = painterResource(id = R.drawable.ic_full_space_mode_switch),
            contentDescription = stringResource(R.string.switch_to_full_space_mode)
        )
    }
}

@Composable
fun HomeSpaceModeIconButton(onClick: () -> Unit, modifier: Modifier = Modifier) {
    FilledTonalIconButton(onClick = onClick, modifier = modifier) {
        Icon(
            painter = painterResource(id = R.drawable.ic_home_space_mode_switch),
            contentDescription = stringResource(R.string.switch_to_home_space_mode)
        )
    }
}

@PreviewLightDark
@Composable
fun ImuContentPreview() {
    NativeSensorAccessTheme {
        ImuContent(
            uiState = SensorUiState(
                accelSample = ImuSample(0.123f, -9.810f, 0.456f, 1234567f),
                gyroSample = ImuSample(0.012f, -0.034f, 0.056f, 1234567f),
                stats = ImuStats(500f, 2.5f, 500f, 2.3f),
                metadata = ImuMetadata(2000, 300, 2000, 300),
                isImuRunning = true
            ),
            onAccelSelected = {},
            onGyroSelected = {},
            modifier = Modifier.padding(24.dp)
        )
    }
}

@Preview(showBackground = true)
@Composable
fun FullSpaceModeButtonPreview() {
    NativeSensorAccessTheme {
        FullSpaceModeIconButton(onClick = {})
    }
}

@PreviewLightDark
@Composable
fun HomeSpaceModeButtonPreview() {
    NativeSensorAccessTheme {
        HomeSpaceModeIconButton(onClick = {})
    }
}
