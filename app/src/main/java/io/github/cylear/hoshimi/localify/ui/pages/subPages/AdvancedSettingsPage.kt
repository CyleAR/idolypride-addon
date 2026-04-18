package io.github.cylear.hoshimi.localify.ui.pages.subPages

import io.github.cylear.hoshimi.localify.ui.components.IPGroupBox
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
import io.github.cylear.hoshimi.localify.MainActivity
import io.github.cylear.hoshimi.localify.R
import io.github.cylear.hoshimi.localify.getConfigState
import io.github.cylear.hoshimi.localify.models.BreastCollapsibleBoxViewModel
import io.github.cylear.hoshimi.localify.models.BreastCollapsibleBoxViewModelFactory
import io.github.cylear.hoshimi.localify.models.IdolyprideConfig
import io.github.cylear.hoshimi.localify.ui.components.IPSwitch


@Composable
fun AdvanceSettingsPage(modifier: Modifier = Modifier,
             context: MainActivity? = null,
             previewData: IdolyprideConfig? = null,
             bottomSpacerHeight: Dp = 120.dp,
             screenH: Dp = 1080.dp) {
    val config = getConfigState(context, previewData)

    val breastParamViewModel: BreastCollapsibleBoxViewModel =
        viewModel(factory = BreastCollapsibleBoxViewModelFactory(initiallyExpanded = false))
    val keyBoardOptionsDecimal = remember {
        KeyboardOptions(keyboardType = KeyboardType.Decimal)
    }

    LazyColumn(modifier = modifier
        .sizeIn(maxHeight = screenH)
        .fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // ?�?� 카메???�정 ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�
        item {
            IPGroupBox(modifier, stringResource(R.string.camera_settings)) {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    IPSwitch(modifier, stringResource(R.string.enable_free_camera), checked = config.value.enableFreeCamera) {
                            v -> context?.onEnableFreeCameraChanged(v)
                    }
                }
            }
            Spacer(Modifier.height(6.dp))
        }

        // ?�?� ?�버�?/ 고급 ?�정 ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�
        item {
            IPGroupBox(modifier, stringResource(R.string.debug_settings)) {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    IPSwitch(modifier, stringResource(R.string.useMasterDBTrans), checked = config.value.useMasterTrans) {
                        v -> context?.onUseMasterTransChanged(v)
                    }
                    IPSwitch(modifier, stringResource(R.string.text_hook_test_mode), checked = config.value.textTest) {
                        v -> context?.onTextTestChanged(v)
                    }
                    IPSwitch(modifier, stringResource(R.string.export_text), checked = config.value.dumpText) {
                        v -> context?.onDumpTextChanged(v)
                    }
                    IPSwitch(modifier, stringResource(R.string.force_export_resource), checked = config.value.forceExportResource) {
                        v -> context?.onForceExportResourceChanged(v)
                    }
                    IPSwitch(modifier, "Login as iOS", checked = config.value.loginAsIOS) {
                        v -> context?.onLoginAsIOSChanged(v)
                    }
                }
            }
            Spacer(Modifier.height(6.dp))
        }

        // ?�?� ?�스??모드: LIVE (dbgMode ?�용) ?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�?�
        item {
            if (config.value.dbgMode) {
                IPGroupBox(modifier, stringResource(R.string.test_mode_live)) {
                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        IPSwitch(modifier, stringResource(R.string.unlockAllLive),
                            checked = config.value.unlockAllLive) {
                                v -> context?.onUnlockAllLiveChanged(v)
                        }
                        IPSwitch(modifier, stringResource(R.string.unlockAllLiveCostume),
                            checked = config.value.unlockAllLiveCostume) {
                                v -> context?.onUnlockAllLiveCostumeChanged(v)
                        }
                    }
                }
                Spacer(Modifier.height(6.dp))
            }
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
