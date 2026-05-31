#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"

void app_main(void)
{
    tusb_init();

    while (1)
    {
        tud_task();

        if (tud_video_n_streaming(0))
        {
            uint8_t frame[320];

            for (int i = 0; i < sizeof(frame); i++)
            {
                frame[i] = i;
            }

            tud_video_n_frame_xfer(0, 0, frame, sizeof(frame));
        }

        vTaskDelay(pdMS_TO_TICKS(33)); // ~30 FPS
    }
}