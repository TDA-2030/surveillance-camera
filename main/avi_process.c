
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "string.h"
#include "esp_log.h"
#include "file_manage.h"
#include "avi_def.h"

static const char *TAG = "avi pro";


 static void back_fill_data(FILE *fp, int width, int height, int fps)
 {
     AVI_RIFF_HEAD riff_head = 
     {
         {'R', 'I', 'F', 'F'},     
         4 + sizeof(AVI_HDRL_LIST) + sizeof(AVI_LIST_HEAD) + nframes * 8 + totalsize,  
         {'A', 'V', 'I', ' '}
     };
 
     AVI_HDRL_LIST hdrl_list = 
     {
         {'L', 'I', 'S', 'T'},
         sizeof(AVI_HDRL_LIST) - 8,
         {'h', 'd', 'r', 'l'},
         {
             {'a', 'v', 'i', 'h'},
             sizeof(AVI_AVIH_CHUNK) - 8,       
             1000000 / fps, 25000, 0, 0, nframes, 0, 1, 100000, width, height, 
             {0, 0, 0, 0}
         },
         {
             {'L', 'I', 'S', 'T'},
             sizeof(AVI_STRL_LIST) - 8,
             {'s', 't', 'r', 'l'},
             {
                 {'s', 't', 'r', 'h'},
                 sizeof(AVI_STRH_CHUNK) - 8,
                 {'v', 'i', 'd', 's'},
                 {'J', 'P', 'E', 'G'},
                 0, 0, 0, 0, 1, 23, 0, nframes, 100000, 0xFFFFFF, 0,
                 {0, 0, width, height}
             },
             {
                 {'s', 't', 'r', 'f'},
                 sizeof(AVI_STRF_CHUNK) - 8,
                 sizeof(AVI_STRF_CHUNK) - 8,
                 width, height, 1, 24,
                 {'J', 'P', 'E', 'G'},
                 width * height * 3, 0, 0, 0, 0
             }
         }
     };
 
     AVI_LIST_HEAD movi_list_head = 
     {
         {'L', 'I', 'S', 'T'},     
         4 + nframes * 8 + totalsize,           
         {'m', 'o', 'v', 'i'}    
     };
 
     //定位到文件头，回填各块数据
     fseek(fp, 0, SEEK_SET);
     fwrite(&riff_head, sizeof(riff_head), 1, fp);
     fwrite(&hdrl_list, sizeof(hdrl_list), 1, fp);
     fwrite(&movi_list_head, sizeof(movi_list_head), 1, fp);
}

void jpeg2avi_start(FILE *fp)
{
    int offset1 = sizeof(AVI_RIFF_HEAD);  //riff head大小
    int offset2 = sizeof(AVI_HDRL_LIST);  //hdrl list大小 
    int offset3 = sizeof(AVI_LIST_HEAD);  //movi list head大小

    //AVI文件偏移量设置到movi list head后，从该位置向后依次写入JPEG数据
    fseek(fp, offset1 + offset2 + offset3, SEEK_SET); 

    //初始化链表
    list_head_init(&list);

    nframes = 0;
    totalsize = 0;
}

void jpeg2avi_add_frame(FILE *fp, void *data, unsigned int len)
{
    unsigned char tmp[4] = {'0', '0', 'd', 'c'};  //00dc = 压缩的视频数据
    struct ListNode *node = (struct ListNode *)malloc(sizeof(struct ListNode));

    /*JPEG图像大小4字节对齐*/
    while (len % 4)
    {
        len++;
    }

    fwrite(tmp, 4, 1, fp);    //写入是否是压缩的视频数据信息    
    fwrite(&len, 4, 1, fp);   //写入4字节对齐后的JPEG图像大小
    fwrite(data, len, 1, fp); //写入真正的JPEG数据

    nframes += 1;
    totalsize += len;

    /*将4字节对齐后的JPEG图像大小保存在链表中*/
    if (node != NULL)
    {
        node->value = len;
        list_add_tail(&node->head, &list);
    }
}

void jpeg2avi_end(FILE *fp, int width, int height, int fps)
{ 
    //写索引块
    write_index_chunk(fp);

    //从文件头开始，回填各块数据
    back_fill_data(fp, width, height, fps);
}


FILE *avifile;

static esp_err_t start_avi() {

  ESP_LOGI(TAG, "Starting an avi ");
    
  char fname[64];

  fm_mkdir("/sdcard/vedio");
  strcpy(fname, "/sdcard/vedio/recoder.avi");

  avifile = fopen(fname, "w");
  if (avifile == NULL)  {
    ESP_LOGE(TAG, "Could not open file");
    return ESP_FAIL;
  }


} // end of start avi

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  another_save_avi runs on cpu 1, saves another frame to the avi file
//
//  the "baton" semaphore makes sure that only one cpu is using the camera subsystem at a time
//

static esp_err_t another_save_avi() {

  xSemaphoreTake( baton, portMAX_DELAY );

  if (fb_in == fb_out) {        // nothing to do

    xSemaphoreGive( baton );
    nothing_avi++;

  } else {

    fb_out = (fb_out + 1) % fb_max;

    int fblen;
    fblen = fb_q[fb_out]->len;

    //xSemaphoreGive( baton );

    if (BlinkWithWrite) {
      digitalWrite(33, LOW);
    }

    jpeg_size = fblen;
    movi_size += jpeg_size;
    uVideoLen += jpeg_size;

    bw = millis();
    size_t dc_err = fwrite(dc_buf, 1, 4, avifile);
    size_t ze_err = fwrite(zero_buf, 1, 4, avifile);

    //bw = millis();

    int time_to_give_up = 0;
    while (ESP.getFreeHeap() < 35000) {
      Serial.print(time_to_give_up); Serial.print(" Low on heap "); Serial.print(ESP.getFreeHeap());
      Serial.print(" frame q = "); ESP_LOGI(TAG, (fb_in + fb_max - fb_out) % fb_max);
      if (time_to_give_up++ == 50) break;
      delay(100 + 5 * time_to_give_up);
    }

    ///Serial.print(fblen); Serial.print(" ");
    //Serial.print (fb_q[fb_out]->buf[fblen-3],HEX );  Serial.print(":");
    ///Serial.print (fb_q[fb_out]->buf[fblen-2],HEX );  Serial.print(":");
    ///Serial.print (fb_q[fb_out]->buf[fblen-1],HEX );  //Serial.print(":");
    //Serial.print (fb_q[fb_out]->buf[fblen  ],HEX );  Serial.print(":");
    ///ESP_LOGI(TAG, "");

    size_t err = fwrite(fb_q[fb_out]->buf, 1, fb_q[fb_out]->len, avifile);

    time_to_give_up = 0;
    while (err != fb_q[fb_out]->len) {
      Serial.print("Error on avi write: err = "); Serial.print(err);
      Serial.print(" len = "); ESP_LOGI(TAG, fb_q[fb_out]->len);
      time_to_give_up++;
      if (time_to_give_up == 10) major_fail();
      Serial.print(time_to_give_up); Serial.print(" Low on heap !!! "); ESP_LOGI(TAG, ESP.getFreeHeap());

      delay(1000);
      size_t err = fwrite(fb_q[fb_out]->buf, 1, fb_q[fb_out]->len, avifile);

    }

    //totalw = totalw + millis() - bw;

    //xSemaphoreTake( baton, portMAX_DELAY );
    esp_camera_fb_return(fb_q[fb_out]);     // release that buffer back to the camera system
    xSemaphoreGive( baton );

    remnant = (4 - (jpeg_size & 0x00000003)) & 0x00000003;

    print_quartet(idx_offset, idxfile);
    print_quartet(jpeg_size, idxfile);

    idx_offset = idx_offset + jpeg_size + remnant + 8;

    jpeg_size = jpeg_size + remnant;
    movi_size = movi_size + remnant;
    if (remnant > 0) {
      size_t rem_err = fwrite(zero_buf, 1, remnant, avifile);
    }

    fileposition = ftell (avifile);       // Here, we are at end of chunk (after padding)
    fseek(avifile, fileposition - jpeg_size - 4, SEEK_SET);    // Here we are the the 4-bytes blank placeholder

    print_quartet(jpeg_size, avifile);    // Overwrite placeholder with actual frame size (without padding)

    fileposition = ftell (avifile);

    // ver97 This is not strictly necessay, so lets get rid of get
    ///  fseek(avifile, fileposition + 6, SEEK_SET);    // Here is the FOURCC "JFIF" (JPEG header) - Overwrite "JFIF" (still images) with more appropriate "AVI1"
    ///  size_t av_err = fwrite(avi1_buf, 1, 4, avifile);
    ///  fileposition = ftell (avifile);
    ///  fseek(avifile, fileposition + jpeg_size - 10 , SEEK_SET);

    fseek(avifile, fileposition + jpeg_size  , SEEK_SET);

    totalw = totalw + millis() - bw;

    digitalWrite(33, HIGH);

  }
} // end of another_pic_avi

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  end_avi runs on cpu 1, empties the queue of frames, writes the index, and closes the files
//

static esp_err_t end_avi() {

  unsigned long current_end = 0;

  other_cpu_active = 0 ;  // shuts down the picture taking program

  //Serial.print(" Write Q: "); Serial.print((fb_in + fb_max - fb_out) % fb_max); Serial.print(" in/out  "); Serial.print(fb_in); Serial.print(" / "); ESP_LOGI(TAG, fb_out);

  for (int i = 0; i < fb_max; i++) {           // clear the queue
    another_save_avi();
  }

  //Serial.print(" Write Q: "); Serial.print((fb_in + fb_max - fb_out) % fb_max); Serial.print(" in/out  "); Serial.print(fb_in); Serial.print(" / "); ESP_LOGI(TAG, fb_out);

  current_end = ftell (avifile);

  ESP_LOGI(TAG, "End of avi - closing the files");

  elapsedms = millis() - startms;
  float fRealFPS = (1000.0f * (float)frame_cnt) / ((float)elapsedms) * xspeed;
  float fmicroseconds_per_frame = 1000000.0f / fRealFPS;
  uint8_t iAttainedFPS = round(fRealFPS);
  uint32_t us_per_frame = round(fmicroseconds_per_frame);

  //Modify the MJPEG header from the beginning of the file, overwriting various placeholders

  fseek(avifile, 4 , SEEK_SET);
  print_quartet(movi_size + 240 + 16 * frame_cnt + 8 * frame_cnt, avifile);

  fseek(avifile, 0x20 , SEEK_SET);
  print_quartet(us_per_frame, avifile);

  unsigned long max_bytes_per_sec = movi_size * iAttainedFPS / frame_cnt;

  fseek(avifile, 0x24 , SEEK_SET);
  print_quartet(max_bytes_per_sec, avifile);

  fseek(avifile, 0x30 , SEEK_SET);
  print_quartet(frame_cnt, avifile);

  fseek(avifile, 0x8c , SEEK_SET);
  print_quartet(frame_cnt, avifile);

  fseek(avifile, 0x84 , SEEK_SET);
  print_quartet((int)iAttainedFPS, avifile);

  fseek(avifile, 0xe8 , SEEK_SET);
  print_quartet(movi_size + frame_cnt * 8 + 4, avifile);

  ESP_LOGI(TAG, F("\n*** Video recorded and saved ***\n"));
  Serial.print(F("Recorded "));
  Serial.print(elapsedms / 1000);
  Serial.print(F("s in "));
  Serial.print(frame_cnt);
  Serial.print(F(" frames\nFile size is "));
  Serial.print(movi_size + 12 * frame_cnt + 4);
  Serial.print(F(" bytes\nActual FPS is "));
  Serial.print(fRealFPS, 2);
  Serial.print(F("\nMax data rate is "));
  Serial.print(max_bytes_per_sec);
  Serial.print(F(" byte/s\nFrame duration is "));  Serial.print(us_per_frame);  ESP_LOGI(TAG, F(" us"));
  Serial.print(F("Average frame length is "));  Serial.print(uVideoLen / frame_cnt);  ESP_LOGI(TAG, F(" bytes"));
  Serial.print("Average picture time (ms) "); ESP_LOGI(TAG,  1.0 * totalp / frame_cnt);
  Serial.print("Average write time (ms)   "); ESP_LOGI(TAG,  totalw / frame_cnt );
  Serial.print("Frames Skipped % ");  ESP_LOGI(TAG,  100.0 * skipped / frame_cnt, 2 );
  Serial.print("Normal jpg % ");  ESP_LOGI(TAG,  100.0 * normal_jpg / frame_cnt, 1 );
  Serial.print("Extend jpg % ");  ESP_LOGI(TAG,  100.0 * extend_jpg / frame_cnt, 1 );
  Serial.print("Bad    jpg % ");  ESP_LOGI(TAG,  100.0 * bad_jpg / total_frames, 1 );

  ESP_LOGI(TAG, "Writing the index");

  fseek(avifile, current_end, SEEK_SET);

  fclose(idxfile);

  size_t i1_err = fwrite(idx1_buf, 1, 4, avifile);

  print_quartet(frame_cnt * 16, avifile);

  idxfile = fopen("/sdcard/idx.tmp", "r");

  if (idxfile != NULL)  {
    //Serial.printf("File open: %s\n", "/sdcard/idx.tmp");
  }  else  {
    ESP_LOGI(TAG, "Could not open file");
    //major_fail();
  }

  char * AteBytes;
  AteBytes = (char*) malloc (8);

  for (int i = 0; i < frame_cnt; i++) {
    size_t res = fread ( AteBytes, 1, 8, idxfile);
    size_t i1_err = fwrite(dc_buf, 1, 4, avifile);
    size_t i2_err = fwrite(zero_buf, 1, 4, avifile);
    size_t i3_err = fwrite(AteBytes, 1, 8, avifile);
  }

  free(AteBytes);
  fclose(idxfile);
  fclose(avifile);
  int xx = remove("/sdcard/idx.tmp");

  ESP_LOGI(TAG, "---");

}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Make the avi move in 4 pieces
//
// make_avi() called in every loop, which calls below, depending on conditions
//   start_avi() - open the file and write headers
//   another_pic_avi() - write one more frame of movie
//   end_avi() - write the final parameters and close the file

void make_avi( ) {

#ifdef include_pir_and_touch

  if (PIRenabled == 1) {
    PIRstatus = digitalRead(PIRpin) + digitalRead(PIRpin) + digitalRead(PIRpin) ;
    //ESP_LOGI(TAG, millis());
    if (DeepSleepPir == 1 && millis() < 15000 ) {
      //DeepSleepPir = 0;
      PIRstatus  = 3;
    }
    //Serial.print("Mak>> "); ESP_LOGI(TAG, PIRstatus);
    if (PIRstatus == 3) {

      if (PIRrecording == 1) {
        // keep recording for 15 more seconds
        if ( (millis() - startms) > (total_frames * capture_interval - 5000)  ) {

          total_frames = total_frames + 10000 / capture_interval ;
          //Serial.print("Make PIR frames = "); ESP_LOGI(TAG, total_frames);
          Serial.print("@");
          //ESP_LOGI(TAG, "Add another 10 seconds");
        }

      } else {

        if ( recording == 0 && newfile == 0) {

          //start a pir recording with current parameters, except no repeat and 15 seconds
          ESP_LOGI(TAG, "Start a PIR");
          PIRrecording = 1;
          repeat = 0;
          total_frames = 15000 / capture_interval;
          xlength = total_frames * capture_interval / 1000;
          recording = 1;
        }
      }
    }
  }

#endif

  // we are recording, but no file is open

  if (newfile == 0 && recording == 1) {                                     // open the file

    digitalWrite(33, HIGH);
    newfile = 1;

    if (EnableBOT == 1 && Internet_Enabled == 1) {           //  if BOT is enabled wait to send it ... could be several seconds (5 or 10)
      //89 config_camera();
      send_a_telegram = 1;
      Wait_for_bot = 1;

      while (Wait_for_bot == 1) {
        delay(1000);
        Serial.print("z");                      // serial monitor will shows these "z" mixed with "*" from telegram sender
      }
    }
    ESP_LOGI(TAG, " ");

    if (delete_old_files) delete_old_stuff();
    
    start_avi();                                 // now start the avi

  } else {

    // we have a file open, but not recording

    if (newfile == 1 && recording == 0) {                                  // got command to close file

      digitalWrite(33, LOW);
      end_avi();

      ESP_LOGI(TAG, "Done capture due to command");

      frames_so_far = total_frames;

      newfile = 0;    // file is closed
      recording = 0;  // DO NOT start another recording
      PIRrecording = 0;

    } else {

      if (newfile == 1 && recording == 1) {                            // regular recording

        if ((millis() - startms) > (total_frames * capture_interval)) {  // time is up, even though we have not done all the frames

          ESP_LOGI TAG, (" "); ESP_LOGI(TAG, "Done capture for time");
          Serial.print("Time Elapsed: "); Serial.print(millis() - startms); Serial.print(" Frames: "); ESP_LOGI(TAG, frame_cnt);
          Serial.print("Config:       "); Serial.print(total_frames * capture_interval ) ; Serial.print(" (");
          Serial.print(total_frames); Serial.print(" x "); Serial.print(capture_interval);  ESP_LOGI(TAG, ")");

          digitalWrite(33, LOW);                                                       // close the file

          end_avi();

          frames_so_far = 0;
          newfile = 0;          // file is closed
          if (repeat > 0) {
            recording = 1;        // start another recording
            repeat = repeat - 1;
            xTaskNotifyGive(AviWriterTask);
          } else {
            recording = 0;
            PIRrecording = 0;
          }

        } else  {                                                            // regular

          another_save_avi();

        }
      }
    }
  }
}

void codeForAviWriterTask( void * parameter )
{
  uint32_t ulNotifiedValue;
  Serial.print("aviwriter, core ");  Serial.print(xPortGetCoreID());
  Serial.print(", priority = "); ESP_LOGI(TAG, uxTaskPriorityGet(NULL));

  for (;;) {
    ulNotifiedValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    while (ulNotifiedValue-- > 0)  {
      make_avi();
      count_avi++;
      delay(1);
    }
  }
}

