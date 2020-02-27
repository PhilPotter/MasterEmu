/* MasterEmu manage states screen source code file
   copyright Phil Potter, 2019 */

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

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

/**
 * This class acts as the manage states screen of the app.
 */
public class ManageStatesActivity extends Activity {

    // define instance variables
    private ControllerSelection selectionObj;
    private long timeSinceLastAnaloguePress = 0;
    private ControllerTextView managestates_wipe_states_button;

    /**
     * This method creates the manage states screen.
     */
    @SuppressWarnings("deprecation")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.managestates_activity);

        StringBuilder managestatesText = new StringBuilder();
        String line = new String();
        BufferedReader bufferedStream = null;

        try {
            bufferedStream = new BufferedReader(new InputStreamReader(getAssets().open("managestates.txt")));

            while ((line = bufferedStream.readLine()) != null) {
                managestatesText.append(line);
                managestatesText.append('\n');
            }
        }
        catch (IOException e) {
            Log.e("ManageStatesActivity:", "Encountered error reading managestates file: " + e.toString());
        }
        finally {
            try {
                bufferedStream.close();
            }
            catch (IOException e) {
                Log.e("ManageStatesActivity:", "Encountered error closing managestates file: " + e.toString());
            }
        }

        TextView managestatesView = (TextView)findViewById(R.id.managestates_blurb);
        managestatesView.setText(managestatesText);

        ButtonColourListener bcl = new ButtonColourListener();
        ControllerTextView managestates_import_states_button = (ControllerTextView)findViewById(R.id.managestates_import_states_button);
        Intent managestates_import_states_Intent = new Intent(this, FileBrowser.class);
        managestates_import_states_Intent.putExtra("actionType", "import_states");
        managestates_import_states_button.setIntent(managestates_import_states_Intent);
        managestates_import_states_button.setOnTouchListener(bcl);
        ControllerTextView managestates_export_states_button = (ControllerTextView)findViewById(R.id.managestates_export_states_button);
        Intent managestates_export_states_Intent = new Intent(this, FileBrowser.class);
        managestates_export_states_Intent.putExtra("actionType", "export_states");
        managestates_export_states_button.setIntent(managestates_export_states_Intent);
        managestates_export_states_button.setOnTouchListener(bcl);
        managestates_wipe_states_button = (ControllerTextView)findViewById(R.id.managestates_wipe_states_button);
        managestates_wipe_states_button.setOnTouchListener(bcl);

        // Set all buttons to same width
        int longest = 0;
        managestates_import_states_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        managestates_export_states_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        managestates_wipe_states_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        if (managestates_import_states_button.getMeasuredWidth() > longest)
            longest = managestates_import_states_button.getMeasuredWidth();
        if (managestates_export_states_button.getMeasuredWidth() > longest)
            longest = managestates_export_states_button.getMeasuredWidth();
        if (managestates_wipe_states_button.getMeasuredWidth() > longest)
            longest = managestates_wipe_states_button.getMeasuredWidth();
        managestates_import_states_button.setLayoutParams(new LinearLayout.LayoutParams(longest, managestates_import_states_button.getMeasuredHeight()));
        managestates_export_states_button.setLayoutParams(new LinearLayout.LayoutParams(longest, managestates_export_states_button.getMeasuredHeight()));
        managestates_wipe_states_button.setLayoutParams(new LinearLayout.LayoutParams(longest, managestates_wipe_states_button.getMeasuredHeight()));

        // Load drawables for apply button
        Drawable light = null;
        Drawable dark = null;
        int lightText, darkText;
        if (android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            lightText = getResources().getColor(R.color.text_colour);
            darkText = getResources().getColor(R.color.text_greyed_out);
        } else {
            lightText = getResources().getColor(R.color.text_colour, null);
            darkText = getResources().getColor(R.color.text_greyed_out, null);
        }
        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.LOLLIPOP_MR1) {
            dark = getResources().getDrawable(R.drawable.view_greyed_out_border);
            light = getResources().getDrawable(R.drawable.view_border);
        } else {
            dark = getResources().getDrawable(R.drawable.view_greyed_out_border, null);
            light = getResources().getDrawable(R.drawable.view_border, null);
        }
        managestates_import_states_button.setActiveDrawable(dark);
        managestates_import_states_button.setInactiveDrawable(light);
        managestates_import_states_button.setHighlightedTextColour(darkText);
        managestates_import_states_button.setUnhighlightedTextColour(lightText);
        managestates_export_states_button.setActiveDrawable(dark);
        managestates_export_states_button.setInactiveDrawable(light);
        managestates_export_states_button.setHighlightedTextColour(darkText);
        managestates_export_states_button.setUnhighlightedTextColour(lightText);
        managestates_wipe_states_button.setActiveDrawable(dark);
        managestates_wipe_states_button.setInactiveDrawable(light);
        managestates_wipe_states_button.setHighlightedTextColour(darkText);
        managestates_wipe_states_button.setUnhighlightedTextColour(lightText);

        // Create selection object and add mappings to it.
        selectionObj = new ControllerSelection();
        selectionObj.addMapping(managestates_import_states_button);
        selectionObj.addMapping(managestates_export_states_button);
        selectionObj.addMapping(managestates_wipe_states_button);

        // Set focus
        View managestates_title = findViewById(R.id.managestates_title);
        managestates_title.requestFocus();

        // Set new MasterEmuMotionListener
        View managestates_root = findViewById(R.id.managestates_root);
        managestates_root.setOnGenericMotionListener(new MasterEmuMotionListener());
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
                    if (selectionObj.getSelected() != managestates_wipe_states_button) {
                        selectionObj.activate();
                    }
                    else {
                        // create dialogue
                        AlertDialog wipeMenu = new AlertDialog.Builder(ManageStatesActivity.this).create();
                        wipeMenu.setTitle("Wipe Prompt");
                        wipeMenu.setMessage("Are you sure you want to wipe all save states and saves?");
                        ManageStatesActivity.WipeListener wl = new ManageStatesActivity.WipeListener();
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
            return ManageStatesActivity.this.motionEvent(event);
        }
    }

    /**
     * This shows a message.
     * @param message
     */
    public void showMessage(String message) {
        Toast messageToast = Toast.makeText(this, message, Toast.LENGTH_SHORT);
        messageToast.show();
    }

    /**
     * This is the listener which handles wiping of all states.
     */
    private class WipeListener implements DialogInterface.OnClickListener {
        @Override
        public void onClick(DialogInterface dialog, int which) {
            switch (which) {
                case DialogInterface.BUTTON_POSITIVE:
                    StateIO manager = new StateIO();
                    boolean result = manager.deleteAllStates(getFilesDir().getAbsolutePath());

                    // check success/failure
                    if (result) {
                        Toast success = Toast.makeText(ManageStatesActivity.this, "Wiped states successfully", Toast.LENGTH_SHORT);
                        success.show();
                    } else {
                        Toast failure = Toast.makeText(ManageStatesActivity.this, "Couldn't wipe states", Toast.LENGTH_SHORT);
                        failure.show();
                    }
                    break;
                case DialogInterface.BUTTON_NEGATIVE:
                    ManageStatesActivity.this.showMessage("Wipe of states was cancelled");
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
                if (cv != managestates_wipe_states_button) {
                    cv.activate();
                }
                else {
                    // create dialogue
                    AlertDialog wipeMenu = new AlertDialog.Builder(ManageStatesActivity.this).create();
                    wipeMenu.setTitle("Wipe Prompt");
                    wipeMenu.setMessage("Are you sure you want to wipe all save states and saves?");
                    ManageStatesActivity.WipeListener wl = new ManageStatesActivity.WipeListener();
                    wipeMenu.setButton(DialogInterface.BUTTON_POSITIVE, "YES", wl);
                    wipeMenu.setButton(DialogInterface.BUTTON_NEGATIVE, "NO", wl);
                    wipeMenu.show();
                }
            }

            return true;
        }
    }
}
