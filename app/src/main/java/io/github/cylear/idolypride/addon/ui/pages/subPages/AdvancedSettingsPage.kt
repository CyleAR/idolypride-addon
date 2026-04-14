package io.github.cylear.idolypride.addon.ui.pages.subPages

import io.github.cylear.idolypride.addon.ui.components.GakuGroupBox
import android.content.res.Configuration.UI_MODE_NIGHT_NO
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.sizeIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import io.github.cylear.idolypride.addon.MainActivity
import io.github.cylear.idolypride.addon.R
import io.github.cylear.idolypride.addon.getConfigState
import io.github.cylear.idolypride.addon.models.BreastCollapsibleBoxViewModel
import io.github.cylear.idolypride.addon.models.BreastCollapsibleBoxViewModelFactory
import io.github.cylear.idolypride.addon.models.IdolyprideConfig
import io.github.cylear.idolypride.addon.ui.components.GakuSwitch


@Composable
fun AdvanceSettingsPage(modifier: Modifier = Modifier,
             context: MainActivity? = null,
             previewData: IdolyprideConfig? = null,
             bottomSpacerHeight: Dp = 120.dp,
             screenH: Dp = 1080.dp) {
    val config = getConfigState(context, previewData)
    // val scrollState = rememberScrollState()

    val breastParamViewModel: BreastCollapsibleBoxViewModel =
        viewModel(factory = BreastCollapsibleBoxViewModelFactory(initiallyExpanded = false))
    val keyBoardOptionsDecimal = remember {
        KeyboardOptions(keyboardType = KeyboardType.Decimal)
    }

    LazyColumn(modifier = modifier
        .sizeIn(maxHeight = screenH)
        // .fillMaxHeight()
        // .verticalScroll(scrollState)
        .fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        item {
            GakuGroupBox(modifier, stringResource(R.string.basic_settings)) {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    GakuSwitch(modifier, stringResource(R.string.enable_plugin), checked = config.value.enabled) {
                        v -> context?.onEnabledChanged(v)
                    }
                }
            }
            Spacer(Modifier.height(6.dp))
        }

        item {
            GakuGroupBox(modifier, stringResource(R.string.camera_settings)) {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    GakuSwitch(modifier, stringResource(R.string.enable_free_camera), checked = config.value.enableFreeCamera) {
                            v -> context?.onEnableFreeCameraChanged(v)
                    }
                }
            }

            Spacer(Modifier.height(6.dp))
        }

        item {
            GakuGroupBox(modifier, stringResource(R.string.debug_settings)) {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    GakuSwitch(modifier, stringResource(R.string.unlockAllLive), checked = config.value.unlockAllLive) {
                        v -> context?.onUnlockAllLiveChanged(v)
                    }
                    GakuSwitch(modifier, "Login as iOS", checked = config.value.loginAsIOS) {
                        v -> context?.onLoginAsIOSChanged(v)
                    }
                    GakuSwitch(modifier, stringResource(R.string.text_hook_test_mode), checked = config.value.textTest) {
                        v -> context?.onTextTestChanged(v)
                    }
                    GakuSwitch(modifier, stringResource(R.string.export_text), checked = config.value.dumpText) {
                        v -> context?.onDumpTextChanged(v)
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
fun AdvanceSettingsPagePreview(modifier: Modifier = Modifier, data: IdolyprideConfig = IdolyprideConfig()) {
    AdvanceSettingsPage(modifier, previewData = data)
}
