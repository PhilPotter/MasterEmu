/* MasterEmu pause screen source code file
   copyright Phil Potter, 2024 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.widget.LinearLayout;
import android.view.KeyEvent;
import android.widget.Toast;
import android.view.InputDevice;
import android.util.Log;

import org.libsdl.app.SDLActivity;

/**
 * This class acts as the pause screen of the app.
 */
public class PauseActivity extends Activity {

    // define instance variables
    private ControllerSelection selectionObj;
    private long timeSinceLastAnaloguePress = 0;
    private static final int sleepTime = 300;

    // native methods to call
    public native void saveStateStub(long emulatorContainerPointer, String fileName);
    public native void resizeAndAudioStub(long emulatorContainerPointer);
    public native void quitStub(long emulatorContainerPointer);

    public void quitEmulator(long emulatorContainerPointer) {
        quitStub(emulatorContainerPointer);
        SDLActivity.mSingleton.finish();
    }

    /**
     * This method sets the screen orientation when locked.
     */
    @Override
    protected void onStart() {
        super.onStart();
        if (OptionStore.orientation_lock) {
            if (OptionStore.orientation.equals("portrait")) {
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
            } else if (OptionStore.orientation.equals("landscape")) {
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
            }
        } else {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR);
        }
    }

    /**
     * This returns to the title screen for us when back is pressed.
     */
    @Override
    public void onBackPressed() {
        long lowerAddress = PauseActivity.this.getIntent().getBundleExtra("MAIN_BUNDLE").getInt("lowerAddress");
        long higherAddress = PauseActivity.this.getIntent().getBundleExtra("MAIN_BUNDLE").getInt("higherAddress");
        long emulatorContainerPointer = ((0xFFFFFFFF00000000L & (higherAddress << 32)) | (0xFFFFFFFFL & lowerAddress));
        quitEmulator(emulatorContainerPointer);
        try {
            Thread.sleep(PauseActivity.sleepTime);
        } catch (Exception e) {
            Log.e("PauseActivity", "Couldn't sleep thread");
        }
        finish();
    }

    /**
     * This method saves the extra bundle and allows us to handle screen
     * reorientations properly.
     */
    @Override
    public void onSaveInstanceState(Bundle savedInstanceState) {
        // transfer mappings
        Bundle extra = getIntent().getBundleExtra("MAIN_BUNDLE");
        savedInstanceState.putAll(extra);
    }

    /**
     * This method creates the pause screen.
     */
    @SuppressWarnings("deprecation")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.pause_activity);

        // Add listeners
        ButtonColourListener bcl = new ButtonColourListener();
        ControllerTextView pause_resume_button = (ControllerTextView)findViewById(R.id.pause_resume_button);
        ControllerTextView pause_loadstate_button = (ControllerTextView)findViewById(R.id.pause_loadstate_button);
        ControllerTextView pause_savestate_button = (ControllerTextView)findViewById(R.id.pause_savestate_button);
        ControllerTextView pause_exit_button = (ControllerTextView)findViewById(R.id.pause_exit_button);
        pause_resume_button.setOnTouchListener(bcl);
        pause_loadstate_button.setOnTouchListener(bcl);
        pause_savestate_button.setOnTouchListener(bcl);
        pause_exit_button.setOnTouchListener(bcl);

        // Set all buttons to same width
        int longest = 0;
        pause_resume_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        pause_loadstate_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        pause_savestate_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        pause_exit_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        if (pause_resume_button.getMeasuredWidth() > longest)
            longest = pause_resume_button.getMeasuredWidth();
        if (pause_loadstate_button.getMeasuredWidth() > longest)
            longest = pause_loadstate_button.getMeasuredWidth();
        if (pause_savestate_button.getMeasuredWidth() > longest)
            longest = pause_savestate_button.getMeasuredWidth();
        if (pause_exit_button.getMeasuredWidth() > longest)
            longest = pause_exit_button.getMeasuredWidth();
        pause_resume_button.setLayoutParams(new LinearLayout.LayoutParams(longest, pause_resume_button.getMeasuredHeight()));
        pause_loadstate_button.setLayoutParams(new LinearLayout.LayoutParams(longest, pause_loadstate_button.getMeasuredHeight()));
        pause_savestate_button.setLayoutParams(new LinearLayout.LayoutParams(longest, pause_savestate_button.getMeasuredHeight()));
        pause_exit_button.setLayoutParams(new LinearLayout.LayoutParams(longest, pause_exit_button.getMeasuredHeight()));

        // Set button drawables
        Drawable light = null;
        Drawable dark = null;
        int lightText, darkText;
        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.LOLLIPOP_MR1) {
            dark = getResources().getDrawable(R.drawable.view_greyed_out_border);
            light = getResources().getDrawable(R.drawable.view_border);
        } else {
            dark = getResources().getDrawable(R.drawable.view_greyed_out_border, null);
            light = getResources().getDrawable(R.drawable.view_border, null);
        }
        if (android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            darkText = getResources().getColor(R.color.text_greyed_out);
            lightText = getResources().getColor(R.color.text_colour);
        } else {
            darkText = getResources().getColor(R.color.text_greyed_out, null);
            lightText = getResources().getColor(R.color.text_colour, null);
        }
        pause_resume_button.setActiveDrawable(dark);
        pause_resume_button.setInactiveDrawable(light);
        pause_resume_button.setHighlightedTextColour(darkText);
        pause_resume_button.setUnhighlightedTextColour(lightText);
        pause_loadstate_button.setActiveDrawable(dark);
        pause_loadstate_button.setInactiveDrawable(light);
        pause_loadstate_button.setHighlightedTextColour(darkText);
        pause_loadstate_button.setUnhighlightedTextColour(lightText);
        pause_savestate_button.setActiveDrawable(dark);
        pause_savestate_button.setInactiveDrawable(light);
        pause_savestate_button.setHighlightedTextColour(darkText);
        pause_savestate_button.setUnhighlightedTextColour(lightText);
        pause_exit_button.setActiveDrawable(dark);
        pause_exit_button.setInactiveDrawable(light);
        pause_exit_button.setHighlightedTextColour(darkText);
        pause_exit_button.setUnhighlightedTextColour(lightText);

        // Create selection objects
        selectionObj = new ControllerSelection();
        selectionObj.addMapping(pause_resume_button);
        selectionObj.addMapping(pause_loadstate_button);
        selectionObj.addMapping(pause_savestate_button);
        selectionObj.addMapping(pause_exit_button);

        // restore bundle if necessary
        if (savedInstanceState != null) {
            Bundle b = new Bundle();
            b.putAll(savedInstanceState);
            getIntent().putExtra("MAIN_BUNDLE", b);
        }

        // Set focus
        pause_resume_button.requestFocus();

        // Set new MasterEmuMotionListener
        View pause_root = findViewById(R.id.pause_root);
        pause_root.setOnGenericMotionListener(new MasterEmuMotionListener());
    }

    /**
     * This class allows us to animate a button.
     */
    protected class ButtonColourListener implements View.OnTouchListener {

        /**
         * This is the actual implementation code.
         */
        @Override
        public boolean onTouch(View v, MotionEvent event) {

            ControllerTextView cv = (ControllerTextView)v;
            if (event.getAction() == MotionEvent.ACTION_DOWN) {
                cv.highlight();
            } else if (event.getAction() == MotionEvent.ACTION_UP) {
                cv.unHighlight();
                handleButton(cv);
            }

            return true;
        }
    }

    public void saveStateSucceeded() {
        Toast success = Toast.makeText(PauseActivity.this, "Successfully saved state", Toast.LENGTH_SHORT);
        success.show();
    }

    public void saveStateFailed() {
        Toast failed = Toast.makeText(PauseActivity.this, "Failed to save state", Toast.LENGTH_SHORT);
        failed.show();
    }

    public void handleButton(View v) {
        // This is where we deal with the button presses.
        if (v.getId() == R.id.pause_resume_button) {
            finish();
        } else if (v.getId() == R.id.pause_loadstate_button) {
            Intent loadStateIntent = new Intent(PauseActivity.this, StateBrowser.class);
            Bundle b = PauseActivity.this.getIntent().getBundleExtra("MAIN_BUNDLE");
            loadStateIntent.putExtra("MAIN_BUNDLE", b);
            startActivity(loadStateIntent);
        } else if (v.getId() == R.id.pause_savestate_button) {
            long lowerAddress = PauseActivity.this.getIntent().getBundleExtra("MAIN_BUNDLE").getInt("lowerAddress");
            long higherAddress = PauseActivity.this.getIntent().getBundleExtra("MAIN_BUNDLE").getInt("higherAddress");
            long emulatorContainerPointer = ((0xFFFFFFFF00000000L & (higherAddress << 32)) | (0xFFFFFFFFL & lowerAddress));

            String strSaveStateFileName = EmuUtils.fileNameWithDateTime(".mesav");                    

            saveStateStub(emulatorContainerPointer, strSaveStateFileName);
        } else if (v.getId() == R.id.pause_exit_button) {
            long lowerAddress = PauseActivity.this.getIntent().getBundleExtra("MAIN_BUNDLE").getInt("lowerAddress");
            long higherAddress = PauseActivity.this.getIntent().getBundleExtra("MAIN_BUNDLE").getInt("higherAddress");
            long emulatorContainerPointer = ((0xFFFFFFFF00000000L & (higherAddress << 32)) | (0xFFFFFFFFL & lowerAddress));
            quitEmulator(emulatorContainerPointer);
            try {
                Thread.sleep(PauseActivity.sleepTime);
            } catch (Exception e) {
                Log.e("PauseActivity", "Couldn't sleep thread");
            }
            finish();
        }
    }

    /**
     * This method helps us to detect gamepad events and do the right thing with them.
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        int action, keycode;
        action = event.getAction();
        keycode = event.getKeyCode();
        boolean returnVal = false;
        if (action == KeyEvent.ACTION_DOWN) {
            if (keycode == KeyEvent.KEYCODE_DPAD_DOWN) {
                if (selectionObj != null)
                    selectionObj.moveDown();
                returnVal = true;
            } else if (keycode == KeyEvent.KEYCODE_DPAD_UP) {
                if (selectionObj != null)
                    selectionObj.moveUp();
                returnVal = true;
            } else if (keycode == KeyEvent.KEYCODE_BUTTON_A) {
                if (selectionObj != null) {
                    ControllerTextView cv = (ControllerTextView)selectionObj.getSelected();
                    if (cv != null) {
                        cv.unHighlight();
                        handleButton(cv);
                    }
                }
                returnVal = true;
            } else if (keycode == KeyEvent.KEYCODE_BUTTON_B) {
                long lowerAddress = PauseActivity.this.getIntent().getBundleExtra("MAIN_BUNDLE").getInt("lowerAddress");
                long higherAddress = PauseActivity.this.getIntent().getBundleExtra("MAIN_BUNDLE").getInt("higherAddress");
                long emulatorContainerPointer = ((0xFFFFFFFF00000000L & (higherAddress << 32)) | (0xFFFFFFFFL & lowerAddress));
                quitEmulator(emulatorContainerPointer);
                try {
                    Thread.sleep(PauseActivity.sleepTime);
                } catch (Exception e) {
                    Log.e("PauseActivity", "Couldn't sleep thread");
                }
                finish();
                returnVal = true;
            }
        }

        if (returnVal)
            return returnVal;

        return super.dispatchKeyEvent(event);
    }

    /**
     * This method deals with input from the analogue stick and some d-pads.
     */
    public boolean motionEvent(MotionEvent event) {
        boolean returnVal = true;

        if (event.getActionMasked() == MotionEvent.ACTION_MOVE) {
            InputDevice dev = event.getDevice();
            if ((event.getSource() & InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD) {
                float y = event.getAxisValue(MotionEvent.AXIS_HAT_Y);

                if (y == -1.0f) {
                    if (selectionObj != null)
                        selectionObj.moveUp();
                } else if (y == 1.0f) {
                    if (selectionObj != null)
                        selectionObj.moveDown();
                }
            } else if ((event.getSource() & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) {
                InputDevice.MotionRange yRange = dev.getMotionRange(MotionEvent.AXIS_Y);
                float yHat = event.getAxisValue(MotionEvent.AXIS_HAT_Y);
                if (yRange == null)
                    return true;
                else {
                    if (event.getEventTime() - timeSinceLastAnaloguePress > 85) {
                        timeSinceLastAnaloguePress = event.getEventTime();
                        float yVal = event.getAxisValue(MotionEvent.AXIS_Y);
                        if (Math.abs(yVal) <= yRange.getMax() / 2)
                            yVal = 0;

                        if (yVal < 0 || yHat < 0) {
                            if (selectionObj != null)
                                selectionObj.moveUp();
                        } else if (yVal > 0 || yHat > 0) {
                            if (selectionObj != null)
                                selectionObj.moveDown();
                        }
                    }
                }
            }
        }

        return returnVal;
    }

    /**
     * This inner class allows us to support joysticks with a listener.
     */
    protected class MasterEmuMotionListener implements View.OnGenericMotionListener {
        @Override
        public boolean onGenericMotion(View v, MotionEvent event) {
            return PauseActivity.this.motionEvent(event);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }
}
