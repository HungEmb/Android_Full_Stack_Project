package com.hvu.hvuapp;

import android.app.Activity;
import android.os.Bundle;
import android.widget.SeekBar;
import android.util.Log;
import android.os.ServiceManager;

import android.os.IHvuledsService;

public class MainActivity extends Activity {
    private static final String DTAG = "HVUapp";
    private IHvuledsService oHVU = null;

    private SeekBar seekBarRED;
    private SeekBar seekBarGREEN;
    private SeekBar seekBarBLUE;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        oHVU = IHvuledsService.Stub.asInterface(ServiceManager.getService("hvuleds"));

        seekBarRED = (SeekBar) findViewById(R.id.seekBarRED);
        seekBarRED.setMax(255);

        seekBarGREEN = (SeekBar) findViewById(R.id.seekBarGREEN);
        seekBarGREEN.setMax(255);

        seekBarBLUE = (SeekBar) findViewById(R.id.seekBarBLUE);
        seekBarBLUE.setMax(255);

        seekBarRED.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
        int valRED = 0;

            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                valRED = progress;
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                try {
                    Log.d(DTAG, "Adjusting brightness off led RED");
                    oHVU.setLed(0, valRED);
                }
                catch (Exception e) {
                    Log.d(DTAG, "FAILED to call service");
                    e.printStackTrace();
                }
            }
        });

        seekBarGREEN.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            int valGREEN = 0;

            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                valGREEN = progress;
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                try {
                    Log.d(DTAG, "Adjusting brightness off led GREEN");
                    oHVU.setLed(1, valGREEN);
                }
                catch (Exception e) {
                    Log.d(DTAG, "FAILED to call service");
                    e.printStackTrace();
                }
            }
        });

        seekBarBLUE.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            int valBLUE = 0;

            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                valBLUE = progress;
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                try {
                    Log.d(DTAG, "Adjusting brightness off led BLUE");
                    oHVU.setLed(2, valBLUE);
                }
                catch (Exception e) {
                    Log.d(DTAG, "FAILED to call service");
                    e.printStackTrace();
                }

           }
        });

    }
}
