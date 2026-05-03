#include "stm32f10x.h"
#include <string.h>
#include "motor.h"
#include "servo.h"
#include "OV7670.h"
#include "LCD_Driver.h"
#include "flash.h"
#include "bluetooth.h"
#include "Delay.h"
#include "adc_sensor.h"

// ===================== 工作模式 =====================
#define MODE_IDLE       0
#define MODE_MANUAL     1
#define MODE_RECORD     2
#define MODE_REPLAY     3

// ===================== 全局变量 =====================
static uint8_t  g_mode        = MODE_IDLE;
static uint8_t  g_recording   = 0;
static uint8_t  g_replaying   = 0;
static uint8_t  g_photoCnt    = 0;
static uint16_t g_loopCnt     = 0;

static uint32_t g_motorWrAddr = 0x000000;
static uint16_t g_motorRecCnt = 0;

static uint8_t  g_imgBuf[CAM_IMG_SIZE];
static uint8_t  motorCmd[6] = {0};

static uint8_t  prevCmd[6] = {0xFF};
static uint32_t last_change_tick = 0;
static uint8_t  rec_first_frame = 1;
static uint8_t  rec_photo_pending = 0;


static uint8_t  idle_drawn = 0;
static uint8_t  g_power_saved = 0;      // 是否进行过掉电保存
static uint8_t  g_has_recorded_data = 0; // 是否有录制数据

#define REC_FLAG_PHOTO   0x01
#define FLASH_MOTOR_LIMIT   0x10000

// ===================== 拍照确认显示（拍照成功后显示1秒）=====================
static void Do_Photo_Confirm(void)
{
	uint8_t cnt = g_photoCnt;
	OV7670_DoPhoto(g_imgBuf, &cnt);
	if (cnt > g_photoCnt) {
		// 拍照成功
		g_photoCnt = cnt;
		LCD_ShowGrayImage(g_imgBuf, CAM_IMG_WIDTH, CAM_IMG_HEIGHT);
		Delay_ms(1000);
	}
	// 恢复当前模式界面
	switch (g_mode) {
		case MODE_MANUAL: UI_ManualLabels(); break;
		case MODE_RECORD: UI_RecordLabels(); break;
		case MODE_REPLAY: UI_ReplayLabels(); break;
		default: idle_drawn = 0; break;
	}
}

// ===================== 轻量提示（不打断当前界面）=====================
static void Toast(const char *msg, uint16_t color)
{
	LCD_RefreshString(TX(0), TY(7), msg, 16 * 8, color, LCD_BLACK);
	Delay_ms(300);
	LCD_ClearRegion(TX(0), TY(7), 16 * 8, 16, LCD_BLACK);
}

// ===================== 错误提示（短暂显示后恢复当前界面）=====================
static void Show_Error(const char *line1, const char *line2)
{
	Lcd_Clear(LCD_BLACK);
	LCD_ShowString(TX(0), TY(0), "ERROR!", LCD_RED, LCD_BLACK);
	LCD_ShowString(TX(0), TY(2), line1, LCD_WHITE, LCD_BLACK);
	if (line2) LCD_ShowString(TX(0), TY(4), line2, LCD_WHITE, LCD_BLACK);
	Delay_ms(500);

	// 恢复当前模式界面
	switch (g_mode) {
		case MODE_MANUAL: UI_ManualLabels(); break;
		case MODE_RECORD: UI_RecordLabels(); break;
		case MODE_REPLAY: UI_ReplayLabels(); break;
		default: idle_drawn = 0; break;
	}
}

// ===================== 拍照封装（拍照后重绘当前界面）=====================
static void Do_Photo(void)
{
	Do_Photo_Confirm();
}

// ===================== 录制停止时：记录最后一帧剩余时间 + 全零停止帧 =====================
static void Rec_WriteStopFrame(void)
{
	if (g_motorRecCnt == 0) return;  // 没录过数据，不需要停止帧

	uint32_t now = g_tick_now();

	// 1. 记录最后一帧（prevCmd）从上次操作到现在还剩多少时间
	uint32_t elapsed = now - last_change_tick;
	if (elapsed > 65535) elapsed = 65535;
	if (elapsed == 0) elapsed = 1;

	uint8_t rec[10];
	rec[0] = prevCmd[0]; rec[1] = prevCmd[1];
	rec[2] = prevCmd[2]; rec[3] = prevCmd[3];
	rec[4] = prevCmd[4]; rec[5] = prevCmd[5];
	rec[6] = (elapsed >> 8) & 0xFF;
	rec[7] = elapsed & 0xFF;
	rec[8] = 0;
	rec[9] = 0;

	if ((g_motorWrAddr & 0xFFF) == 0)
		Flash_EraseSector(g_motorWrAddr);
	Flash_Write(g_motorWrAddr, rec, 10);
	g_motorWrAddr += 10;
	g_motorRecCnt++;

	// 2. 写一帧全零停止帧（持续时间1ms，立即停止）
	uint8_t stop[10] = {0, 0, 0, 0, 0, 0, 0, 1, 0, 0};

	if ((g_motorWrAddr & 0xFFF) == 0)
		Flash_EraseSector(g_motorWrAddr);
	Flash_Write(g_motorWrAddr, stop, 10);
	g_motorWrAddr += 10;
	g_motorRecCnt++;
}

// ===================== 主函数 =====================
int main(void)
{
	uint32_t jedec;
	uint32_t idle_start_tick = 0;
	uint8_t  in_idle = 0;

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	Tick_Init();

	PWM_Init();
	SG90_Init();
	Flash_Init();
	Bluetooth_Init();
	Lcd_Init();

	// 开机显示 Flash ID
	jedec = Flash_ReadJEDEC();
	Lcd_Clear(LCD_BLACK);
	LCD_ShowString(TX(0), TY(0), "STM32 Robot", LCD_GREEN, LCD_BLACK);
	LCD_ShowNum(TX(0), TY(2), (uint32_t)jedec, 8, LCD_WHITE, LCD_BLACK);
	Delay_ms(2000);

	// 掉电恢复
	{
		SavedState st;
		if (State_Load(&st) && st.motorRecCnt > 0) {
			g_motorRecCnt = st.motorRecCnt;
			g_loopCnt     = st.loopCnt;
			g_photoCnt    = st.photoCnt;
		} else {
			State_Clear();
		}
	}

	// ===================== 主循环 =====================
	while (1)
	{
		process_received_data();

		// ---- 空闲计时与休眠 ----
		if (model_flag_1 || model_flag_2 || model_flag_3 ||
		    model_flag_PHOTO || model_flag_PHOTOData ||
		    model_flag_ADC || model_flag_RETURN)
		{
			idle_start_tick = g_tick_now();
			in_idle = 0;
		}

		if (g_mode == MODE_IDLE && !in_idle) {
			in_idle = 1;
			idle_start_tick = g_tick_now();
		} else if (g_mode != MODE_IDLE) {
			in_idle = 0;
		}

		if (in_idle && (g_tick_now() - idle_start_tick >= 60000)) {
			Enter_Sleep();
			idle_drawn = 0;  // 休眠回来重绘
			idle_start_tick = g_tick_now();
		}

		// ===================== 模式切换 =====================
		if (model_flag_1) {
			model_flag_1 = 0;
			if (g_mode == MODE_IDLE) {
				g_mode = MODE_MANUAL;
				State_Clear();
				UI_ManualLabels();
				idle_drawn = 0;
			} else {
				Show_Error("Exit to IDLE", "first!");
			}
		}
		if (model_flag_2) {
			model_flag_2 = 0;
			if (g_mode == MODE_IDLE) {
				g_mode = MODE_RECORD;
				g_recording = 1;
				State_Clear();
				rec_first_frame = 1;
				rec_photo_pending = 0;
				g_recPhotoCnt = 0;
				g_motorWrAddr = 0x000000;
				g_motorRecCnt = 0;
				g_loopCnt = 0;
				Flash_EraseSector(0x000000);
				// 清除 MANUAL 模式遗留的照片
				g_photoCnt = 0;
				for (uint32_t a = FLASH_PHOTO_BASE; a < FLASH_PHOTO_LIMIT; a += 0x1000)
					Flash_EraseSector(a);
				UI_RecordLabels();
				UI_SetRecording(1);
				idle_drawn = 0;
			} else {
				Show_Error("Exit to IDLE", "first!");
			}
		}
		if (model_flag_3) {
			model_flag_3 = 0;
			if (g_mode == MODE_IDLE) {
				g_mode = MODE_REPLAY;
				g_replaying = 1;
				// 先扫描录制数据中的拍照次数（擦除前）
				Flash_ScanPhotoFlags();
				g_photoCnt = 0;
				for (uint32_t a = FLASH_PHOTO_BASE; a < FLASH_PHOTO_LIMIT; a += 0x1000)
					Flash_EraseSector(a);
				UI_ReplayLabels();
				idle_drawn = 0;
			} else {
				Show_Error("Exit to IDLE", "first!");
			}
		}

		if (model_flag_PHOTO) {
			model_flag_PHOTO = 0;
			if (g_mode == MODE_RECORD && g_recording) {
				if (g_recPhotoCnt >= MAX_PHOTOS) {
					Toast("Photos FULL!", LCD_RED);
				} else {
					rec_photo_pending = 1;
					g_recPhotoCnt++;
					UI_SetPhotoPending(1);
				}
			} else if (g_mode == MODE_MANUAL) {
				if (g_photoCnt >= MAX_PHOTOS) {
					Toast("Photos FULL!", LCD_RED);
				} else {
					Do_Photo();
				}
			}
			// IDLE / REPLAY 模式下忽略拍照指令
		}

		if (model_flag_PHOTOData) {
			model_flag_PHOTOData = 0;
			if (g_photoCnt > 0) {
				uint32_t addr = FLASH_PHOTO_BASE + (uint32_t)(g_photoCnt - 1) * PHOTO_SIZE;
				Flash_Read(addr, g_imgBuf, CAM_IMG_SIZE);
				Send_Image(g_imgBuf, CAM_IMG_WIDTH, CAM_IMG_HEIGHT);
			}
		}

		if (model_flag_RETURN) {
			model_flag_RETURN = 0;
			if (g_mode == MODE_RECORD) {
				// 退出录制前：记录最后一帧持续时间 + 写全零停止帧
				Rec_WriteStopFrame();
				if (g_motorRecCnt > 0)
					g_has_recorded_data = 1;
			}
			if (g_mode == MODE_REPLAY) {
				TIM_Cmd(TIM4, DISABLE);
				Tick_RestoreFreeRun();
				Motor_Stop();
				g_replaying = 0;
			}
			g_mode = MODE_IDLE;
			Motor_Stop();
			idle_drawn = 0;  // 回到空闲时重绘
		}

		if (model_flag_ADC) {
			model_flag_ADC = 0;
			if (g_mode == MODE_IDLE) {
				if (g_motorRecCnt > 0) {
					State_Save(g_motorRecCnt, g_photoCnt, g_photoCnt, g_loopCnt);
					g_power_saved = 1;
					Lcd_Clear(LCD_BLACK);
					LCD_ShowString(TX(0), TY(0), "SAVED!", LCD_GREEN, LCD_BLACK);
					LCD_ShowString(TX(0), TY(2), "Loop:", LCD_WHITE, LCD_BLACK);
					LCD_ShowNum(TX(5), TY(2), g_loopCnt, 3, LCD_WHITE, LCD_BLACK);
					LCD_ShowString(TX(0), TY(4), "Steps:", LCD_WHITE, LCD_BLACK);
					LCD_ShowNum(TX(6), TY(4), g_motorRecCnt, 4, LCD_WHITE, LCD_BLACK);
					LCD_ShowString(TX(0), TY(6), "Power off OK", LCD_YELLOW, LCD_BLACK);
					Delay_ms(1000);
					idle_drawn = 0;  // 恢复 IDLE 界面
				}
			}
		}

		// ==================== 各模式执行 ====================
		switch (g_mode)
		{
		// ==================== 手动模式 ====================
		case MODE_MANUAL:
			joystick_to_motor_control(motorCmd);
			pwm_out(motorCmd);
			SG90_SetAngle(motorCmd);

			UI_ManualRefreshLCD(motorCmd[1], motorCmd[3],
			                    motorCmd[4], motorCmd[5], g_photoCnt);
			break;

		// ==================== 录制模式 ====================
		// 新逻辑：只在数据变化时操作电机/舵机 + 记录
		// 流程：新数据到达 → 输出新数据 → 记录帧+持续时间 → 重置计时
		case MODE_RECORD:
		{
			joystick_to_motor_control(motorCmd);

			// 检测是否为全新数据
			uint8_t is_new = 0;
			if (rec_first_frame) {
				is_new = 1;
				rec_first_frame = 0;
			} else {
				for (uint8_t ci = 0; ci < 6; ci++) {
					if (motorCmd[ci] != prevCmd[ci]) { is_new = 1; break; }
				}
			}

			if (is_new) {
				// ---- 计算上一帧的持续时间 ----
				uint32_t now = g_tick_now();
				uint32_t elapsed;
				if (g_motorRecCnt == 0) {
					elapsed = 1;  // 首帧，无上一帧，给最小值
				} else {
					elapsed = now - last_change_tick;
					if (elapsed > 65535) elapsed = 65535;
					if (elapsed == 0) elapsed = 1;
				}

				// ---- 拍照标记 ----
				uint8_t photo_flag = 0;
				if (rec_photo_pending) {
					photo_flag = REC_FLAG_PHOTO;
				}
				rec_photo_pending = 0;
				UI_SetPhotoPending(0);

				// ---- 写入当前帧 ----
				if (g_motorWrAddr + 10 > FLASH_MOTOR_LIMIT) {
					g_recording = 0;
					g_has_recorded_data = 1;
					UI_SetRecording(0);
				} else {
					uint8_t rec[10];
					rec[0] = motorCmd[0]; rec[1] = motorCmd[1];
					rec[2] = motorCmd[2]; rec[3] = motorCmd[3];
					rec[4] = motorCmd[4]; rec[5] = motorCmd[5];
					rec[6] = (elapsed >> 8) & 0xFF;
					rec[7] = elapsed & 0xFF;
					rec[8] = photo_flag;
					rec[9] = 0;

					if ((g_motorWrAddr & 0xFFF) == 0)
						Flash_EraseSector(g_motorWrAddr);
					Flash_Write(g_motorWrAddr, rec, 10);
					g_motorWrAddr += 10;
					g_motorRecCnt++;

					// ---- 输出新数据 ----
					pwm_out(motorCmd);
					SG90_SetAngle(motorCmd);

					for (uint8_t ci = 0; ci < 6; ci++) prevCmd[ci] = motorCmd[ci];
					last_change_tick = now;
				}
			}
			// 数据没变 → 不操作电机/舵机，不记录

			// LCD 刷新（始终刷新显示）
			UI_RecordRefreshLCD(g_recording, g_motorRecCnt, g_recPhotoCnt,
			                    motorCmd[0], motorCmd[1],
			                    motorCmd[2], motorCmd[3],
			                    motorCmd[4], motorCmd[5], 0);

			Delay_ms(10);
			break;
		}

		// ==================== 回放模式 ====================
		case MODE_REPLAY:
		{
			static uint8_t  replayBuf[10];
			static uint32_t replayAddr;
			static uint32_t replayIdx;
			int16_t left_disp, right_disp;

			if (g_replaying == 1) {
				Tick_SaveFreeRun();
				replayAddr = 0x000000;
				replayIdx = 0;
				Flash_Read(replayAddr, replayBuf, 10);
				replayAddr += 10;

				// 首帧拍照检测
				if ((replayBuf[8] & REC_FLAG_PHOTO) && g_photoCnt < MAX_PHOTOS) {
					Motor_Stop();
					if (OV7670_Capture(g_imgBuf) == 0) {
						uint32_t paddr = FLASH_PHOTO_BASE + (uint32_t)g_photoCnt * PHOTO_SIZE;
						Flash_EraseSector(paddr);
						Flash_Write(paddr, g_imgBuf, CAM_IMG_SIZE);
						g_photoCnt++;
						LCD_ShowGrayImage(g_imgBuf, CAM_IMG_WIDTH, CAM_IMG_HEIGHT);
						Delay_ms(1000);
						UI_ReplayLabels();
					}
				}

				motorCmd[0] = replayBuf[0]; motorCmd[1] = replayBuf[1];
				motorCmd[2] = replayBuf[2]; motorCmd[3] = replayBuf[3];
				motorCmd[4] = replayBuf[4]; motorCmd[5] = replayBuf[5];
				pwm_out(motorCmd);
				SG90_SetAngle(motorCmd);

				uint16_t dur = ((uint16_t)replayBuf[6] << 8) | replayBuf[7];
				if (dur == 0) dur = 1;

				// 预读下一帧到中断缓冲
				if (replayIdx + 1 < g_motorRecCnt) {
					uint8_t nextBuf[10];
					Flash_Read(replayAddr, nextBuf, 10);
					replay_motor[0] = nextBuf[0]; replay_motor[1] = nextBuf[1];
					replay_motor[2] = nextBuf[2]; replay_motor[3] = nextBuf[3];
					replay_motor[4] = nextBuf[4]; replay_motor[5] = nextBuf[5];
					replay_duration = ((uint16_t)nextBuf[6] << 8) | nextBuf[7];
					replay_step_ready = 1;
				}

				left_disp  = (motorCmd[0] == 0) ? (int16_t)motorCmd[1] : -(int16_t)motorCmd[1];
				right_disp = (motorCmd[2] == 0) ? (int16_t)motorCmd[3] : -(int16_t)motorCmd[3];

				UI_ReplayRefreshLCD(1, g_motorRecCnt, dur, g_photoCnt, g_recPhotoCnt,
				                    left_disp, right_disp,
				                    motorCmd[4], motorCmd[5]);

				g_tick_expired = 0;
				Tick_StartOneShot(dur);
				g_replaying = 2;
			}
			else if (g_replaying == 2)
			{
				process_received_data();

				if (replay_step_ready == 0) {
					replayIdx++;

					// 一圈回放完成
					if (replayIdx >= g_motorRecCnt) {
						while (replay_step_ready == 0 && g_tick_expired == 0)
							process_received_data();

						TIM_Cmd(TIM4, DISABLE);
						Tick_RestoreFreeRun();
						Motor_Stop();
						replay_step_ready = 0;

						g_loopCnt++;
						Lcd_Clear(LCD_BLACK);
						LCD_ShowString(TX(0), TY(0), "LOOP", LCD_YELLOW, LCD_BLACK);
						LCD_ShowNum(TX(5), TY(0), g_loopCnt, 3, LCD_WHITE, LCD_BLACK);
						LCD_ShowString(TX(0), TY(2), "steps:", LCD_WHITE, LCD_BLACK);
						LCD_ShowNum(TX(6), TY(2), replayIdx, 5, LCD_WHITE, LCD_BLACK);
						LCD_ShowString(TX(0), TY(4), "Sending...", LCD_CYAN, LCD_BLACK);

						// 回传照片
						if (g_photoCnt > 0) {
							uint32_t addr = FLASH_PHOTO_BASE + (uint32_t)(g_photoCnt - 1) * PHOTO_SIZE;
							Flash_Read(addr, g_imgBuf, CAM_IMG_SIZE);
							Send_Image(g_imgBuf, CAM_IMG_WIDTH, CAM_IMG_HEIGHT);
						} else {
							USART1_SendByte(0xAA);
							USART1_SendByte(0xBB);
							USART1_SendByte(0x00);
							USART1_SendByte(0x00);
							USART1_SendByte(0xCC);
							USART1_SendByte(0xDD);
						}

						LCD_ShowString(TX(0), TY(6), "Sent!", LCD_GREEN, LCD_BLACK);
						Delay_ms(500);

						// 清空照片，重新开始
						g_photoCnt = 0;
						for (uint32_t a = FLASH_PHOTO_BASE; a < FLASH_PHOTO_LIMIT; a += 0x1000)
							Flash_EraseSector(a);

						Tick_SaveFreeRun();
						replayAddr = 0x000000;
						replayIdx = 0;
						Flash_ScanPhotoFlags();  // 重新扫描录制拍照次数
						g_replaying = 1;
						UI_ReplayLabels();
						break;
					}

					// 读取下一帧
					Flash_Read(replayAddr, replayBuf, 10);
					replayAddr += 10;

					motorCmd[0] = replayBuf[0]; motorCmd[1] = replayBuf[1];
					motorCmd[2] = replayBuf[2]; motorCmd[3] = replayBuf[3];
					motorCmd[4] = replayBuf[4]; motorCmd[5] = replayBuf[5];

					uint16_t dur2 = ((uint16_t)replayBuf[6] << 8) | replayBuf[7];
					if (dur2 == 0) dur2 = 1;

					// 拍照检测
					if ((replayBuf[8] & REC_FLAG_PHOTO) && g_photoCnt < MAX_PHOTOS) {
						Motor_Stop();
						if (OV7670_Capture(g_imgBuf) == 0) {
							uint32_t paddr = FLASH_PHOTO_BASE + (uint32_t)g_photoCnt * PHOTO_SIZE;
							Flash_EraseSector(paddr);
							Flash_Write(paddr, g_imgBuf, CAM_IMG_SIZE);
							g_photoCnt++;
							LCD_ShowGrayImage(g_imgBuf, CAM_IMG_WIDTH, CAM_IMG_HEIGHT);
							Delay_ms(1000);
							UI_ReplayLabels();
						}
					}

					pwm_out(motorCmd);
					SG90_SetAngle(motorCmd);

					replay_motor[0] = motorCmd[0]; replay_motor[1] = motorCmd[1];
					replay_motor[2] = motorCmd[2]; replay_motor[3] = motorCmd[3];
					replay_motor[4] = motorCmd[4]; replay_motor[5] = motorCmd[5];
					replay_duration = dur2;
					replay_step_ready = 1;

					g_tick_expired = 0;
					Tick_StartOneShot(dur2);

					left_disp  = (motorCmd[0] == 0) ? (int16_t)motorCmd[1] : -(int16_t)motorCmd[1];
					right_disp = (motorCmd[2] == 0) ? (int16_t)motorCmd[3] : -(int16_t)motorCmd[3];

					UI_ReplayRefreshLCD(replayIdx + 1, g_motorRecCnt, dur2, g_photoCnt, g_recPhotoCnt,
					                    left_disp, right_disp,
					                    motorCmd[4], motorCmd[5]);
				}
			}
			break;
		}

		// ==================== 空闲模式（局部刷新）====================
		default:
		case MODE_IDLE:
			Motor_Stop();
			if (!idle_drawn) {
				UI_IdleLabels();
				idle_drawn = 1;
			}
			UI_IdleRefreshLCD(g_photoCnt, g_power_saved, g_has_recorded_data);
			Delay_ms(200);
			break;
		}
	}
}
