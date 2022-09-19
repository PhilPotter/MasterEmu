/* MasterEmu controller mapping screen source code file
   copyright Phil Potter, 2022 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;

/**
 * This class acts as the controller mapping screen of the app.
 */
public class ControllerMappingActivity extends Activity {

    // define instance variables
    private ControllerSelection selectionObj;
    private long timeSinceLastAnaloguePress = 0;
    private ControllerTextView controllermapping_map_button;

    // static variable which can be referenced from SDL
    static public boolean controller_mapping_mode = false;

    /**
     * This method creates the manage states screen.
     */
    @SuppressWarnings("deprecation")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.controllermapping_activity);

        StringBuilder controllermappingText = new StringBuilder();
        String line = new String();
        BufferedReader bufferedStream = null;

        try {
            bufferedStream = new BufferedReader(new InputStreamReader(getAssets().open("controllermapping.txt")));

            while ((line = bufferedStream.readLine()) != null) {
                controllermappingText.append(line);
                controllermappingText.append('\n');
            }
        }
        catch (IOException e) {
            Log.e("ControllerMapping:", "Encountered error reading controllermapping file: " + e.toString());
        }
        finally {
            try {
                bufferedStream.close();
            }
            catch (IOException e) {
                Log.e("ControllerMapping:", "Encountered error closing controllermapping file: " + e.toString());
            }
        }

        TextView controllermappingView = (TextView)findViewById(R.id.controllermapping_blurb);
        controllermappingView.setText(controllermappingText);

        ButtonColourListener bcl = new ButtonColourListener();
        controllermapping_map_button = (ControllerTextView)findViewById(R.id.controllermapping_map_button);
        controllermapping_map_button.setIntent(new Intent(ControllerMappingActivity.this, SDLActivity.class));
        controllermapping_map_button.setOnTouchListener(bcl);
        ControllerTextView controllermapping_reset_button = (ControllerTextView)findViewById(R.id.controllermapping_reset_button);
        controllermapping_reset_button.setOnTouchListener(bcl);

        // Set all buttons to same width
        int longest = 0;
        controllermapping_map_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        controllermapping_reset_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        if (controllermapping_map_button.getMeasuredWidth() > longest)
            longest = controllermapping_map_button.getMeasuredWidth();
        if (controllermapping_reset_button.getMeasuredWidth() > longest)
            longest = controllermapping_reset_button.getMeasuredWidth();
        controllermapping_map_button.setLayoutParams(new LinearLayout.LayoutParams(longest, controllermapping_map_button.getMeasuredHeight()));
        controllermapping_reset_button.setLayoutParams(new LinearLayout.LayoutParams(longest, controllermapping_reset_button.getMeasuredHeight()));

        // Load drawables
        Drawable light = null;
        Drawable dark = null;
        int lightText, darkText;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            lightText = getResources().getColor(R.color.text_colour);
            darkText = getResources().getColor(R.color.text_greyed_out);
        } else {
            lightText = getResources().getColor(R.color.text_colour, null);
            darkText = getResources().getColor(R.color.text_greyed_out, null);
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP_MR1) {
            dark = getResources().getDrawable(R.drawable.view_greyed_out_border);
            light = getResources().getDrawable(R.drawable.view_border);
        } else {
            dark = getResources().getDrawable(R.drawable.view_greyed_out_border, null);
            light = getResources().getDrawable(R.drawable.view_border, null);
        }
        controllermapping_map_button.setActiveDrawable(dark);
        controllermapping_map_button.setInactiveDrawable(light);
        controllermapping_map_button.setHighlightedTextColour(darkText);
        controllermapping_map_button.setUnhighlightedTextColour(lightText);
        controllermapping_reset_button.setActiveDrawable(dark);
        controllermapping_reset_button.setInactiveDrawable(light);
        controllermapping_reset_button.setHighlightedTextColour(darkText);
        controllermapping_reset_button.setUnhighlightedTextColour(lightText);

        // Create selection object and add mappings to it.
        selectionObj = new ControllerSelection();
        selectionObj.addMapping(controllermapping_map_button);
        selectionObj.addMapping(controllermapping_reset_button);

        // Set focus
        View controllermapping_title = findViewById(R.id.controllermapping_title);
        controllermapping_title.requestFocus();

        // Set new MasterEmuMotionListener
        View controllermapping_root = findViewById(R.id.controllermapping_root);
        controllermapping_root.setOnGenericMotionListener(new MasterEmuMotionListener());
    }

    /**
     * This method currently just keeps the orientation locked if necessary.
     */
    @Override
    protected void onStart() {
        super.onStart();

        // make sure screen orientation is set here if locked
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
     * This method helps us to detect gamepad events and do the right thing with them.
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        int action, keycode;
        boolean returnVal = false;
        action = event.getAction();
        keycode = event.getKeyCode();
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
                    if (selectionObj.getSelected() == controllermapping_map_button) {
                        ControllerMappingActivity.controller_mapping_mode = true;
                        selectionObj.activate();
                    }
                    else {
                        // create dialogue
                        AlertDialog wipeMenu = new AlertDialog.Builder(ControllerMappingActivity.this).create();
                        wipeMenu.setTitle("Reset Prompt");
                        wipeMenu.setMessage("Are you sure you want to reset the mapping?");
                        ControllerMappingActivity.ResetMappingListener wl = new ControllerMappingActivity.ResetMappingListener();
                        wipeMenu.setButton(DialogInterface.BUTTON_POSITIVE, "YES", wl);
                        wipeMenu.setButton(DialogInterface.BUTTON_NEGATIVE, "NO", wl);
                        wipeMenu.show();
                    }
                }
                returnVal = true;
            } else if (keycode == KeyEvent.KEYCODE_BUTTON_B) {
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
            return ControllerMappingActivity.this.motionEvent(event);
        }
    }

    /**
     * This shows a message.
     * @param message
     */
    private void showMessage(String message) {
        Toast messageToast = Toast.makeText(this, message, Toast.LENGTH_SHORT);
        messageToast.show();
    }

    /**
     * This is the listener which handles wiping of all states.
     */
    private class ResetMappingListener implements DialogInterface.OnClickListener {
        @Override
        public void onClick(DialogInterface dialog, int which) {
            switch (which) {
                case DialogInterface.BUTTON_POSITIVE:
                    try {
                        File buttonMappingFile = new File(getFilesDir() + "/button_mapping.ini");
                        if (buttonMappingFile.exists())
                            buttonMappingFile.delete();
                    }
                    catch (Exception e) {
                        Toast failure = Toast.makeText(ControllerMappingActivity.this, "Couldn't reset mapping", Toast.LENGTH_SHORT);
                        failure.show();
                    }
                    Toast success = Toast.makeText(ControllerMappingActivity.this, "Reset mapping successfully", Toast.LENGTH_SHORT);
                    success.show();
                    break;
                case DialogInterface.BUTTON_NEGATIVE:
                    ControllerMappingActivity.this.showMessage("Reset of mapping was cancelled");
                    break;
            }
        }
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

            ControllerMapped cv = (ControllerMapped) v;
            if (event.getAction() == MotionEvent.ACTION_DOWN) {
                cv.highlight();
            } else if (event.getAction() == MotionEvent.ACTION_UP) {
                cv.unHighlight();
                if (cv == controllermapping_map_button) {
                    ControllerMappingActivity.controller_mapping_mode = true;
                    cv.activate();
                }
                else {
                    // create dialogue
                    AlertDialog wipeMenu = new AlertDialog.Builder(ControllerMappingActivity.this).create();
                    wipeMenu.setTitle("Reset Prompt");
                    wipeMenu.setMessage("Are you sure you want to reset the mapping?");
                    ControllerMappingActivity.ResetMappingListener wl = new ControllerMappingActivity.ResetMappingListener();
                    wipeMenu.setButton(DialogInterface.BUTTON_POSITIVE, "YES", wl);
                    wipeMenu.setButton(DialogInterface.BUTTON_NEGATIVE, "NO", wl);
                    wipeMenu.show();
                }
            }

            return true;
        }
    }
}
