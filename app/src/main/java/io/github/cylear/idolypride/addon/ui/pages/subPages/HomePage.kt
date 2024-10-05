package io.github.cylear.idolypride.addon.ui.pages.subPages

import io.github.cylear.idolypride.addon.ui.components.GakuGroupBox
import android.content.res.Configuration.UI_MODE_NIGHT_NO
import androidx.compose.foundation.clickable
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import io.github.cylear.idolypride.addon.MainActivity
import io.github.cylear.idolypride.addon.R
import io.github.cylear.idolypride.addon.getConfigState
import io.github.cylear.idolypride.addon.getProgramConfigState
import io.github.cylear.idolypride.addon.getProgramDownloadAbleState
import io.github.cylear.idolypride.addon.getProgramDownloadErrorStringState
import io.github.cylear.idolypride.addon.getProgramDownloadState
import io.github.cylear.idolypride.addon.getProgramLocalResourceVersionState
import io.github.cylear.idolypride.addon.hookUtils.FileHotUpdater
import io.github.cylear.idolypride.addon.mainUtils.FileDownloader
import io.github.cylear.idolypride.addon.models.IdolyprideConfig
import io.github.cylear.idolypride.addon.models.ResourceCollapsibleBoxViewModel
import io.github.cylear.idolypride.addon.models.ResourceCollapsibleBoxViewModelFactory
import io.github.cylear.idolypride.addon.ui.components.base.CollapsibleBox
import io.github.cylear.idolypride.addon.ui.components.GakuButton
import io.github.cylear.idolypride.addon.ui.components.GakuProgressBar
import io.github.cylear.idolypride.addon.ui.components.GakuRadio
import io.github.cylear.idolypride.addon.ui.components.GakuSwitch
import io.github.cylear.idolypride.addon.ui.components.GakuTextInput
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

    // val scrollState = rememberScrollState()
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
        // .fillMaxHeight()
        // .verticalScroll(scrollState)
        // .width(IntrinsicSize.Max)
        .fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        item {
            GakuGroupBox(modifier = modifier, stringResource(R.string.basic_settings)) {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    GakuSwitch(modifier, stringResource(R.string.enable_plugin), checked = config.value.enabled) {
                            v -> context?.onEnabledChanged(v)
                    }
                }
            }
            Spacer(Modifier.height(6.dp))
        }

        item {
            GakuGroupBox(modifier = modifier, contentPadding = 0.dp, title = stringResource(R.string.graphic_settings)) {
                LazyColumn(modifier = Modifier
                    .sizeIn(maxHeight = screenH),
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
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

                    item {
                        HorizontalDivider(
                            thickness = 1.dp,
                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
                        )
                    }
                }
            }
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
