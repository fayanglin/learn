package com.iflytek.halcae;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.InputStreamReader;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;

public class MainActivity extends Activity implements OnClickListener {

    protected static final String TAG = "lfy-sharehost";
	private Button btnStartStop;
	private AudioRecordThread mAudioRecordThread;
    private JniHalCae jniHalCae = null;
    
	@Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
		Log.d(TAG, "onCreate");
		btnStartStop = (Button) findViewById(R.id.btn_start_stop);
		btnStartStop.setOnClickListener(this);

		jniHalCae = new JniHalCae();
		jniHalCae.shareReadInit();
        
    }

	
	@Override
	public void onClick(View v) {
		Log.d(TAG, "onClick");
		switch (v.getId()){
		case R.id.btn_start_stop:
			if (mAudioRecordThread!=null && mAudioRecordThread.isRecording()) {
				Log.d(TAG, "stopRecord");
				mAudioRecordThread.stopRecord();
				mAudioRecordThread = null;
				btnStartStop.setText("start record");
				//Toast.makeText(this,filename, Toast.LENGTH_LONG).show();
			}else {
				Log.d(TAG, "startRecord");
				mAudioRecordThread = new AudioRecordThread(jniHalCae,this);
				mAudioRecordThread.start();
				btnStartStop.setText("stop record");
			}
			break;
			
		}
		
	}
	
	
}
