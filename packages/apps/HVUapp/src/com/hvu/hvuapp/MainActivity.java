package com.hvu.hvuapp;

import android.app.Activity;
import android.os.Bundle;
import android.widget.SeekBar;
import android.util.Log;
import android.os.ServiceManager;
import android.content.Context;

import android.os.HvuledsManager;
import android.hardware.hvuleds.V2_0.Led;

public class MainActivity extends Activity {
    private static final String DTAG = "HVUapp";
    private HvuledsManager mHvuledsManager;

    private SeekBar seekBarRED;
    private SeekBar seekBarGREEN;
    private SeekBar seekBarBLUE;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mHvuledsManager = (HvuledsManager) getSystemService(Context.HVULEDS_SERVICE);

        seekBarRED = (SeekBar) findViewById(R.id.seekBarRED);
        seekBarRED.setMax(255);

        seekBarGREEN = (SeekBar) findViewById(R.id.seekBarGREEN);
        seekBarGREEN.setMax(255);

        seekBarBLUE = (SeekBar) findViewById(R.id.seekBarBLUE);
        seekBarBLUE.setMax(255);

        seekBarRED.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                try {
                    mHvuledsManager.setLedBrightness(Led.RED, progress);
                }
                catch (Exception e) {
                    Log.d(DTAG, "FAILED to call service");
                    e.printStackTrace();
                }
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                Log.d(DTAG, "Adjusted brightness off led RED");
            }
        });

        seekBarGREEN.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                try {
                    mHvuledsManager.setLedBrightness(Led.GREEN, progress);
                }
                catch (Exception e) {
                    Log.d(DTAG, "FAILED to call service");
                    e.printStackTrace();
                }
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                Log.d(DTAG, "Adjusted brightness off led GREEN");
            }
        });

        seekBarBLUE.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                try {
                    mHvuledsManager.setLedBrightness(Led.BLUE, progress);
                }
                catch (Exception e) {
                    Log.d(DTAG, "FAILED to call service");
                    e.printStackTrace();
                }
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                Log.d(DTAG, "Adjusted brightness off led BLUE");
            }
        });

    }
}
