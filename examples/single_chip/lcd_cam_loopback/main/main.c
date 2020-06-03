#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "soc/system_reg.h"
#include "esp_timer.h"
#include "cam.h"
#include "ov2640.h"
#include "lcd.h"
#include "jpeg.h"
#include "lssh_forward.h"
#include "image_util.h"
#include "input.h"
#include "lssh_human_face_mn2_q.h"
#include "fb_gfx.h"

static const char *TAG = "main";

#define JPEG_MODE 0

#define CAM_WIDTH   (320)
#define CAM_HIGH    (240)

#define LCD_CLK   GPIO_NUM_15
#define LCD_MOSI  GPIO_NUM_9
#define LCD_DC    GPIO_NUM_13
#define LCD_RST   GPIO_NUM_16
#define LCD_CS    GPIO_NUM_11
#define LCD_BK    GPIO_NUM_6

#define CAM_XCLK  GPIO_NUM_0
#define CAM_PCLK  GPIO_NUM_1
#define CAM_VSYNC GPIO_NUM_3
#define CAM_HSYNC GPIO_NUM_2

#define CAM_D0    GPIO_NUM_46
#define CAM_D1    GPIO_NUM_45
#define CAM_D2    GPIO_NUM_41
#define CAM_D3    GPIO_NUM_42
#define CAM_D4    GPIO_NUM_39
#define CAM_D5    GPIO_NUM_40
#define CAM_D6    GPIO_NUM_21
#define CAM_D7    GPIO_NUM_38

//box_array_t *onet_forward(dl_matrix3du_t *image, box_array_t *net_boxes, net_config_t *config);
//face_id_list id_list = {0};

static void rgb_print(dl_matrix3du_t *image_matrix, uint32_t color, const char *str)
{
    fb_data_t fb;
    fb.width = image_matrix->w;
    fb.height = image_matrix->h;
    fb.data = image_matrix->item;
    fb.bytes_per_pixel = 3;
    fb.format = FB_BGR565;
    fb_gfx_print(&fb, (fb.width - (strlen(str) * 14)) / 2, 10, color, str);
}

static int rgb_printf(dl_matrix3du_t *image_matrix, uint32_t color, const char *format, ...)
{
    char loc_buf[64];
    char *temp = loc_buf;
    int len;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    len = vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
    va_end(copy);
    if (len >= sizeof(loc_buf))
    {
        temp = (char *)malloc(len + 1);
        if (temp == NULL)
        {
            return 0;
        }
    }
    vsnprintf(temp, len + 1, format, arg);
    va_end(arg);
    rgb_print(image_matrix, color, temp);
    if (len > 64)
    {
        free(temp);
    }
    return len;
}
typedef struct
{
    float score;          /// score threshold for filter candidates by score
    float nms;            /// nms threshold for nms process
    int candidate_number; /// candidate number limitation for each net
} threshold_config_t;
typedef struct
{
    int w;                        /// net width
    int h;                        /// net height
    threshold_config_t threshold; /// threshold of net
} net_config_t;
void face_detection(dl_matrix3dq_t *image_mat, uint16_t *cam_buf, lssh_config_t lssh_config, int n) {

    image_resize_n(image_mat->item, cam_buf, 320/n, 240/n, 3, 320, n);

    // int t1 = esp_timer_get_time();
    // int32_t c1 = get_count();
    box_array_t *lnet_boxes = lssh_detect_object(image_mat, lssh_config);
    if (lnet_boxes)
    {
        //box_array_t *net_boxes = onet_forward(image_mat, lnet_boxes, &onet_config);
        draw_rectangle_rgb565(cam_buf, lnet_boxes, CAM_WIDTH);
        dl_lib_free(lnet_boxes->category);
        dl_lib_free(lnet_boxes->score);
        dl_lib_free(lnet_boxes->box);
        dl_lib_free(lnet_boxes->landmark);
        dl_lib_free(lnet_boxes);
        // if (net_boxes)
        // {
        //     ESP_LOGI(TAG, "faces: %d", net_boxes->len);
        //     dl_matrix3du_t *aligned_face = dl_matrix3du_alloc(1, 56, 56, 3);
        //     if (align_face(net_boxes, image_mat, aligned_face) == ESP_OK)
        //     {
        //         if (id_list.count == 0)
        //         {
        //             ESP_LOGI(TAG, "\n>>> Face ID Enrollment Starts <<<\n");
        //             int8_t left_sample_face = enroll_face(&id_list, aligned_face);
        //             ESP_LOGI(TAG, "Face ID Enrollment: Take the %d sample",
        //                      3 - left_sample_face);
        //             if (left_sample_face == 0)
        //             {
        //                 ESP_LOGW(TAG, "\nEnrolled Face ID: %d", id_list.tail);

        //                 if (id_list.count == 1)
        //                 {
        //                     ESP_LOGW(TAG, "\n>>> Face Recognition Starts <<<\n");
        //                 }
        //             }
        //         }
        //         /* Do face recognition */
        //         else
        //         {
        //             int matched_id = recognize_face(&id_list, aligned_face);
        //             if (matched_id >= 0)
        //             {
        //                 draw_rectangle_rgb565(cam_buf, net_boxes, CAM_WIDTH, true);
        //                 ESP_LOGW("Recog", "Matched Face ID: %d\n", matched_id);
        //             }
        //             else
        //             {
        //                 draw_rectangle_rgb565(cam_buf, net_boxes, CAM_WIDTH, false);
        //                 ESP_LOGW("Recog", "No Matched Face ID\n");
        //             }
        //         }
        //     }
        //     dl_matrix3du_free(aligned_face);
        //     dl_lib_free(net_boxes->score);
        //     dl_lib_free(net_boxes->box);
        //     dl_lib_free(net_boxes->landmark);
        //     dl_lib_free(net_boxes);
        // }
    }
    // int32_t c2 = get_count();
    // int t2 = esp_timer_get_time();
    // printf("count: %d, time: %dms\n", (c2 - c1)/1000, (t2 - t1) / 1000);
}

static void cam_task(void *arg)
{
#if 1
    lcd_config_t lcd_config = {
        .clk_fre = 80 * 1000 * 1000,
        .pin_clk = LCD_CLK,
        .pin_mosi = LCD_MOSI,
        .pin_dc = LCD_DC,
        .pin_cs = LCD_CS,
        .pin_rst = LCD_RST,
        .pin_bk = LCD_BK,
        .max_buffer_size = 32 * 1024,
        .horizontal = 2 // 2: UP, 3： DOWN
    };
    lcd_init(&lcd_config);
#endif

    //face_id_init(&id_list, 1, 3);

#if 0

    uint8_t *buf = heap_caps_malloc(CAM_WIDTH * CAM_HIGH * 2, MALLOC_CAP_SPIRAM);

    while (1)
    {
        memcpy(buf, get_input(), CAM_WIDTH * CAM_HIGH * 2);
        face_detection(buf);
        lcd_set_index(0, 0, CAM_WIDTH - 1, CAM_HIGH - 1);
        lcd_write_data(buf, CAM_WIDTH * CAM_HIGH * 2);
        vTaskDelay(100);
    }
    
#else
    cam_config_t cam_config = {
        .bit_width = 8,
        .mode.jpeg = JPEG_MODE,
        .xclk_fre = 16 * 1000 * 1000,
        .pin = {
            .xclk  = CAM_XCLK,
            .pclk  = CAM_PCLK,
            .vsync = CAM_VSYNC,
            .hsync = CAM_HSYNC,
        },
        .pin_data = {CAM_D0, CAM_D1, CAM_D2, CAM_D3, CAM_D4, CAM_D5, CAM_D6, CAM_D7},
        .size = {
            .width = CAM_WIDTH,
            .high  = CAM_HIGH,
        },
        .max_buffer_size = 32 * 1024, // max 32KBytes
        .task_pri = 10
    };

    // 使用PingPang buffer，帧率更高， 也可以单独使用一个buffer节省内存
    cam_config.frame1_buffer = (uint8_t *)heap_caps_malloc(CAM_WIDTH * CAM_HIGH * 2 * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
    cam_config.frame2_buffer = (uint8_t *)heap_caps_malloc(CAM_WIDTH * CAM_HIGH * 2 * sizeof(uint8_t), MALLOC_CAP_SPIRAM);

    cam_init(&cam_config);
    if (OV2640_Init(0, 1) == 1) {
        vTaskDelete(NULL);
        return;
    }
    if (cam_config.mode.jpeg) {
        OV2640_JPEG_Mode();
    } else {
        OV2640_RGB565_Mode(false);	//RGB565模式
    }
    
    OV2640_ImageSize_Set(800, 600);
    OV2640_ImageWin_Set(0, 0, 800, 600);
  	OV2640_OutSize_Set(CAM_WIDTH, CAM_HIGH); 
    ESP_LOGI(TAG, "camera init done\n");
    cam_start();

    dl_matrix3dq_op_init();

    //dl_matrix3du_t *image_mat = dl_matrix3du_alloc(1, CAM_WIDTH, CAM_HIGH, 3);
    int n = 3;

    lssh_config_t lssh_config = lssh_get_config(lssh_human_face_mn2, 16*n, 10, 0.3, 240, 320, false, DL_TIE_IMPL);
    dl_matrix3dq_t *image_mat = dl_matrix3dq_alloc(1, 320/n, 320/n, 3, 0);

    fb_data_t fb;
    fb.width = 320;
    fb.height = 240;
    fb.bytes_per_pixel = 2;

    int last_time = esp_timer_get_time();

    /* Load configuration for detection */
    while (1) {
        uint8_t *cam_buf = NULL;
        size_t recv_len = cam_take(&cam_buf);

        int t1 = esp_timer_get_time();
        face_detection(image_mat, cam_buf, lssh_config, n);
        int t2 = esp_timer_get_time();
        fb.data = cam_buf;
        int fps = 1e6 / (t2 - last_time);
        char buf[10];
        snprintf(buf, 10, "FPS: %d\n", fps);
        fb_gfx_print(&fb, 20, 20, RGB565_MASK_GREEN, buf);
        last_time = t2;

#if JPEG_MODE
        int w, h;
        uint8_t *img = jpeg_decode(cam_buf, &w, &h);
        if (img) {
            printf("jpeg: w: %d, h: %d\n", w, h);
            lcd_set_index(0, 0, w - 1, h - 1);
            lcd_write_data(img, w * h * sizeof(uint16_t));
            free(img);
        }
#else
        lcd_set_index(0, 0, CAM_WIDTH - 1, CAM_HIGH - 1);
        lcd_write_data(cam_buf, CAM_WIDTH * CAM_HIGH * 2);
#endif
        cam_give(cam_buf);   
        // int t3 = esp_timer_get_time();
        // printf("time: %d + %d = %dms, fps: %d, %d\n",
        //         (t2 - t1) / 1000,
        //         (t3 - t2) / 1000,
        //         (t3 - t1) / 1000,
        //         1000000 / (t3 - t1), fps);
        // 使用逻辑分析仪观察帧率
        gpio_set_level(LCD_BK, 1);
        gpio_set_level(LCD_BK, 0);  
    }
#endif
    //dl_matrix3du_free(image_mat);
    vTaskDelete(NULL);
}

void app_main() 
{
    SET_PERI_REG_MASK(SYSTEM_CPU_PER_CONF_REG, SYSTEM_CPU_WAIT_MODE_FORCE_ON); // fix two core issue
    xTaskCreatePinnedToCore(cam_task, "cam_task", 2*4096, NULL, 5, NULL, 1);
}