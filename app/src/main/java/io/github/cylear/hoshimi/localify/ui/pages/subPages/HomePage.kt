package io.github.cylear.hoshimi.localify.ui.pages.subPages

import io.github.cylear.hoshimi.localify.ui.components.IPGroupBox
import android.content.res.Configuration.UI_MODE_NIGHT_NO
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.sizeIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import io.github.cylear.hoshimi.localify.MainActivity
import io.github.cylear.hoshimi.localify.R
import io.github.cylear.hoshimi.localify.getConfigState
import io.github.cylear.hoshimi.localify.getProgramConfigState
import io.github.cylear.hoshimi.localify.getProgramDownloadAbleState
import io.github.cylear.hoshimi.localify.getProgramDownloadErrorStringState
import io.github.cylear.hoshimi.localify.getProgramDownloadState
import io.github.cylear.hoshimi.localify.getProgramLocalResourceVersionState
import io.github.cylear.hoshimi.localify.hookUtils.FileHotUpdater
import io.github.cylear.hoshimi.localify.mainUtils.FileDownloader
import io.github.cylear.hoshimi.localify.models.IdolyprideConfig
import io.github.cylear.hoshimi.localify.models.ResourceCollapsibleBoxViewModel
import io.github.cylear.hoshimi.localify.models.ResourceCollapsibleBoxViewModelFactory
import io.github.cylear.hoshimi.localify.ui.components.IPRadio
import io.github.cylear.hoshimi.localify.ui.components.IPSwitch
import io.github.cylear.hoshimi.localify.ui.components.GakuTextInput
import io.github.cylear.hoshimi.localify.ui.components.IPButton
import io.github.cylear.hoshimi.localify.ui.components.base.CollapsibleBox
import java.io.File


@Composable
fun HomePage(modifier: Modifier = Modifier,
             context: MainActivity? = null,
             previewData: IdolyprideConfig? = null,
             bottomSpacerHeight: Dp = 120.dp,
             screenH: Dp = 1080.dp) {
    val config = getConfigState(context, previewData)
    val programConfig = getProgramConfigState(context)

    val downloadProgress by getProgramDownloadState(context)
    val downloadAble by getProgramDownloadAbleState(context)
    val localResourceVersion by getProgramLocalResourceVersionState(context)
    val downloadErrorString by getProgramDownloadErrorStringState(context)

    val keyboardOptionsNumber = remember {
        KeyboardOptions(keyboardType = KeyboardType.Number)
    }
    val keyBoardOptionsDecimal = remember {
        KeyboardOptions(keyboardType = KeyboardType.Decimal)
    }

    val resourceSettingsViewModel: ResourceCollapsibleBoxViewModel =
        viewModel(factory = ResourceCollapsibleBoxViewModelFactory(initiallyExpanded = false))

    fun onClickDownload() {
        context?.mainPageAssetsViewDataUpdate(
            downloadAbleState = false,
            errorString = "",
            downloadProgressState = -1f
        )
        val (_, newUrl) = FileDownloader.checkAndChangeDownloadURL(programConfig.value.transRemoteZipUrl)
        context?.onPTransRemoteZipUrlChanged(newUrl, 0, 0, 0)
        FileDownloader.downloadFile(
            newUrl,
            checkContentTypes = listOf("application/zip", "application/octet-stream"),
            onDownload = { progress, _, _ ->
                context?.mainPageAssetsViewDataUpdate(downloadProgressState = progress)
            },

            onSuccess = { byteArray ->
                context?.mainPageAssetsViewDataUpdate(
                    downloadAbleState = true,
                    errorString = "",
                    downloadProgressState = -1f
                )
                val file = File(context?.filesDir, "update_trans.zip")
                file.writeBytes(byteArray)
                val newFileVersion = FileHotUpdater.getZipResourceVersion(file.absolutePath)
                if (newFileVersion != null) {
                    context?.mainPageAssetsViewDataUpdate(
                        localResourceVersionState = newFileVersion
                    )
                }
                else {
                    context?.mainPageAssetsViewDataUpdate(
                        localResourceVersionState = context.getString(
                            R.string.invalid_zip_file
                        ),
                        errorString = context.getString(R.string.invalid_zip_file_warn)
                    )
                }
            },

            onFailed = { code, reason ->
                context?.mainPageAssetsViewDataUpdate(
                    downloadAbleState = true,
                    errorString = reason,
                )
            })

    }


    LazyColumn(modifier = modifier
        .sizeIn(maxHeight = screenH)
        .fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // ?�?� 기본 ?�정 ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�
        item {
            IPGroupBox(modifier = modifier, stringResource(R.string.basic_settings)) {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    IPSwitch(modifier, stringResource(R.string.enable_plugin), checked = config.value.enabled) {
                            v -> context?.onEnabledChanged(v)
                    }
                    IPSwitch(modifier, stringResource(R.string.replace_font), checked = config.value.replaceFont) {
                            v -> context?.onReplaceFontChanged(v)
                    }
                }
            }
            Spacer(Modifier.height(6.dp))
        }

        // ?�?� 그래???�정 ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�
        item {
            IPGroupBox(modifier = modifier, contentPadding = 0.dp, title = stringResource(R.string.graphic_settings)) {
                LazyColumn(modifier = Modifier
                    .sizeIn(maxHeight = screenH),
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    // FPS
                    item {
                        Spacer(modifier = Modifier.height(8.dp))
                        GakuTextInput(modifier = modifier
                            .padding(start = 4.dp, end = 4.dp)
                            .height(45.dp)
                            .fillMaxWidth(),
                            fontSize = 14f,
                            value = config.value.targetFrameRate.toString(),
                            onValueChange = { c -> context?.onTargetFpsChanged(c, 0, 0, 0)},
                            label = { Text(stringResource(R.string.setFpsTitle)) },
                            keyboardOptions = keyboardOptionsNumber)
                    }

                    // ?�면 방향
                    item {
                        Column(modifier = Modifier.padding(start = 8.dp, end = 8.dp),
                            verticalArrangement = Arrangement.spacedBy(4.dp)) {
                            Text(stringResource(R.string.orientation_lock))
                            Row(modifier = modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                val radioModifier = remember {
                                    modifier
                                        .height(40.dp)
                                        .weight(1f)
                                }
                                IPRadio(modifier = radioModifier,
                                    text = stringResource(R.string.orientation_orig),
                                    selected = config.value.gameOrientation == 0,
                                    onClick = { context?.onGameOrientationChanged(0) })
                                IPRadio(modifier = radioModifier,
                                    text = stringResource(R.string.orientation_portrait),
                                    selected = config.value.gameOrientation == 1,
                                    onClick = { context?.onGameOrientationChanged(1) })
                                IPRadio(modifier = radioModifier,
                                    text = stringResource(R.string.orientation_landscape),
                                    selected = config.value.gameOrientation == 2,
                                    onClick = { context?.onGameOrientationChanged(2) })
                            }
                        }
                    }

                    item {
                        HorizontalDivider(
                            thickness = 1.dp,
                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
                        )
                    }

                    // 커스?� 그래???�정
                    item {
                        IPSwitch(modifier.padding(start = 8.dp, end = 8.dp),
                            stringResource(R.string.useCustomeGraphicSettings),
                            checked = config.value.useCustomeGraphicSettings) {
                                v -> context?.onUseCustomeGraphicSettingsChanged(v)
                        }

                        CollapsibleBox(modifier = modifier,
                            expandState = config.value.useCustomeGraphicSettings,
                            collapsedHeight = 0.dp,
                            showExpand = false
                        ) {
                            LazyColumn(modifier = modifier
                                .padding(8.dp)
                                .sizeIn(maxHeight = screenH)
                                .fillMaxWidth(),
                                verticalArrangement = Arrangement.spacedBy(12.dp)
                            ) {
                                // ?�리??버튼
                                item {
                                    Row(modifier = modifier.fillMaxWidth(),
                                        horizontalArrangement = Arrangement.spacedBy(2.dp)) {
                                        val btnMod = remember { modifier.height(40.dp).weight(1f) }
                                        IPButton(modifier = btnMod, text = stringResource(R.string.max_high),  onClick = { context?.onChangePresetQuality(4) })
                                        IPButton(modifier = btnMod, text = stringResource(R.string.very_high), onClick = { context?.onChangePresetQuality(3) })
                                        IPButton(modifier = btnMod, text = stringResource(R.string.hign),      onClick = { context?.onChangePresetQuality(2) })
                                        IPButton(modifier = btnMod, text = stringResource(R.string.middle),    onClick = { context?.onChangePresetQuality(1) })
                                        IPButton(modifier = btnMod, text = stringResource(R.string.low),       onClick = { context?.onChangePresetQuality(0) })
                                    }
                                }
                                // RenderScale + QualityLevel
                                item {
                                    Row(modifier = modifier, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                                        val fieldMod = remember { modifier.height(45.dp).weight(1f) }
                                        GakuTextInput(modifier = fieldMod, fontSize = 14f,
                                            value = config.value.renderScale.toString(),
                                            onValueChange = { c -> context?.onRenderScaleChanged(c, 0, 0, 0)},
                                            label = { Text(stringResource(R.string.renderscale)) },
                                            keyboardOptions = keyBoardOptionsDecimal)
                                        GakuTextInput(modifier = fieldMod, fontSize = 14f,
                                            value = config.value.qualitySettingsLevel.toString(),
                                            onValueChange = { c -> context?.onQualitySettingsLevelChanged(c, 0, 0, 0)},
                                            label = { Text("QualityLevel (1/1/2/3/5)") },
                                            keyboardOptions = keyboardOptionsNumber)
                                    }
                                }
                                // VolumeIndex + MaxBufferPixel
                                item {
                                    Row(modifier = modifier, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                                        val fieldMod = remember { modifier.height(45.dp).weight(1f) }
                                        GakuTextInput(modifier = fieldMod, fontSize = 14f,
                                            value = config.value.volumeIndex.toString(),
                                            onValueChange = { c -> context?.onVolumeIndexChanged(c, 0, 0, 0)},
                                            label = { Text("VolumeIndex (0/1/2/3/4)") },
                                            keyboardOptions = keyboardOptionsNumber)
                                        GakuTextInput(modifier = fieldMod, fontSize = 14f,
                                            value = config.value.maxBufferPixel.toString(),
                                            onValueChange = { c -> context?.onMaxBufferPixelChanged(c, 0, 0, 0)},
                                            label = { Text("MaxBufferPixel (1024/1440/2538/3384/8190)", fontSize = 10.sp) },
                                            keyboardOptions = keyboardOptionsNumber)
                                    }
                                }
                                // ReflectionLevel + LodLevel
                                item {
                                    Row(modifier = modifier, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                                        val fieldMod = remember { modifier.height(45.dp).weight(1f) }
                                        GakuTextInput(modifier = fieldMod, fontSize = 14f,
                                            value = config.value.reflectionQualityLevel.toString(),
                                            onValueChange = { c -> context?.onReflectionQualityLevelChanged(c, 0, 0, 0)},
                                            label = { Text("ReflectionLevel (0~5)") },
                                            keyboardOptions = keyboardOptionsNumber)
                                        GakuTextInput(modifier = fieldMod, fontSize = 14f,
                                            value = config.value.lodQualityLevel.toString(),
                                            onValueChange = { c -> context?.onLodQualityLevelChanged(c, 0, 0, 0)},
                                            label = { Text("LOD Level (0~5)") },
                                            keyboardOptions = keyboardOptionsNumber)
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Spacer(Modifier.height(6.dp))
        }

        item {
            Spacer(modifier = modifier.height(bottomSpacerHeight))
        }
    }
}


@Preview(showBackground = true, uiMode = UI_MODE_NIGHT_NO)
@Composable
fun HomePagePreview(modifier: Modifier = Modifier, data: IdolyprideConfig = IdolyprideConfig()) {
    HomePage(modifier, previewData = data)
}
