package com.iflytek.halcae;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Environment;
import android.util.Log;

public class AudioRecordThread extends Thread {

    private static final String TAG = "-lfy-AudioRecordThread";

    private boolean isRecording;
    private int mChannelConfig = AudioFormat.CHANNEL_IN_MONO;
    private int mSampleRate = 16000;
    private int bufferSizeInBytes;
    private JniHalCae jniHalCae;
    private Context mContext;
    
    public AudioRecordThread(JniHalCae jni,Context context) {
    	this.jniHalCae = jni;
    	mContext = context;
    	
        bufferSizeInBytes =3072;// AudioRecord.getMinBufferSize(mSampleRate,
        		//mChannelConfig, AudioFormat.ENCODING_PCM_16BIT); // need to be larger than size of a frame

        Log.d(TAG, "bufferSizeInBytes = " + bufferSizeInBytes);

		AudioManager am = (AudioManager) mContext.getSystemService(mContext.AUDIO_SERVICE);
		if( Integer.parseInt(am.getParameters("audio_record_status").trim()) == 0){
			am.setParameters("audio_record_status=1");
		}
    }

    public boolean isRecording() {
        return this.isAlive() && isRecording;
    }
    
    @Override
    public void run() {
    	startRecord();
    }

	public void stopRecord() {
		isRecording = false;
		AudioManager am = (AudioManager) mContext.getSystemService(mContext.AUDIO_SERVICE);
		if( Integer.parseInt(am.getParameters("audio_record_status").trim()) == 1){
			am.setParameters("audio_record_status=0");
		}
		interrupt();
	}
    
    
    public void startRecord() {

        File tempFile = new File(Environment.getExternalStorageDirectory().getPath(), "alexa.pcm");//临时文件
        
        Log.i(TAG, "start recording,file=" + tempFile.getAbsolutePath());

        BufferedOutputStream bosTemp = null;
        
        try {
        	
        	isRecording = true;
        	
            byte[] buffer = new byte[bufferSizeInBytes];
            
            int total = 0;
            int bufferReadResult = 0;
            bosTemp = new BufferedOutputStream(new FileOutputStream(tempFile.getAbsolutePath()));
            while (isRecording) {
				
                bufferReadResult = jniHalCae.shareReadData(buffer, bufferSizeInBytes);
                if(bufferReadResult > 0){
                	Log.i(TAG, "bufferReadResult = " + bufferReadResult);
                	total += bufferReadResult;
                	bosTemp.write(buffer,0,bufferReadResult);
                }
            }
            bosTemp.flush();
            bosTemp.close();
            bosTemp = null;
            Log.i(TAG, "audio byte total = "  + total);
            Log.i(TAG, "stop recording,file=" + tempFile.getAbsolutePath());
            

        } catch (Exception e) {
            e.printStackTrace();
        } finally {
        	
        	try {

				if(bosTemp != null){
					bosTemp.close();
					bosTemp = null;
				}
			} catch (IOException e) {
				e.printStackTrace();
			}
        }
    }
}
