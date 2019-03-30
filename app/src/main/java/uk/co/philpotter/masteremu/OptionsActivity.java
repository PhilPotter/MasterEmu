/* MasterEmu options screen source code file
   copyright Phil Potter, 2019 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.CheckBox;
import android.content.res.Configuration;
import java.io.File;
import java.io.RandomAccessFile;
import java.io.IOException;
import android.util.Log;
import java.io.BufferedWriter;
import java.io.FileWriter;
import android.widget.Toast;
import android.content.pm.ActivityInfo;
import android.view.InputDevice;

/**
 * This class acts as the options screen of the app.
 */
public class OptionsActivity extends Activity {

    // define instance variables
    private ControllerSelection selectionObj;
    private long timeSinceLastAnaloguePress = 0;

    /**
     * This method creates the options screen.
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.options_activity);

        ButtonColourListener bcl = new ButtonColourListener();
        ControllerTextView options_apply_button = (ControllerTextView)findViewById(R.id.options_apply_button);
        options_apply_button.setOnTouchListener(bcl);

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
        options_apply_button.setActiveDrawable(dark);
        options_apply_button.setInactiveDrawable(light);
        options_apply_button.setHighlightedTextColour(darkText);
        options_apply_button.setUnhighlightedTextColour(lightText);
        ControllerCheckBox orientation_lock = (ControllerCheckBox)findViewById(R.id.orientation_lock);
        ControllerCheckBox disable_sound = (ControllerCheckBox)findViewById(R.id.disable_sound);
        ControllerCheckBox larger_buttons = (ControllerCheckBox)findViewById(R.id.larger_buttons);
        ControllerCheckBox no_buttons = (ControllerCheckBox)findViewById(R.id.no_buttons);
        ControllerCheckBox japanese_mode = (ControllerCheckBox)findViewById(R.id.japanese_mode);
        ControllerCheckBox no_stretching = (ControllerCheckBox)findViewById(R.id.no_stretching);
        ControllerCheckBox game_genie = (ControllerCheckBox)findViewById(R.id.game_genie);
        orientation_lock.setActiveDrawable(dark);
        disable_sound.setActiveDrawable(dark);
        larger_buttons.setActiveDrawable(dark);
        no_buttons.setActiveDrawable(dark);
        japanese_mode.setActiveDrawable(dark);
        no_stretching.setActiveDrawable(dark);
        game_genie.setActiveDrawable(dark);

        // Create selection object and add mappings to it.
        options_apply_button.isOptions();
        selectionObj = new ControllerSelection();
        selectionObj.addMapping(orientation_lock);
        selectionObj.addMapping(disable_sound);
        selectionObj.addMapping(larger_buttons);
        selectionObj.addMapping(no_buttons);
        selectionObj.addMapping(japanese_mode);
        selectionObj.addMapping(no_stretching);
        selectionObj.addMapping(game_genie);
        selectionObj.addMapping(options_apply_button);

        // Set focus
        View options_title = findViewById(R.id.options_title);
        options_title.requestFocus();

        // Set new MasterEmuMotionListener
        View options_root = findViewById(R.id.options_root);
        options_root.setOnGenericMotionListener(new MasterEmuMotionListener());
    }

    /**
     * This method restores the checkbox states amongst other things.
     */
    @Override
    protected void onStart() {
        super.onStart();
        if (OptionStore.orientation_lock) {
            CheckBox orientation_lock = (CheckBox)findViewById(R.id.orientation_lock);
            orientation_lock.setChecked(true);
        }
        if (OptionStore.disable_sound) {
            CheckBox disable_sound = (CheckBox)findViewById(R.id.disable_sound);
            disable_sound.setChecked(true);
        }
        if (OptionStore.larger_buttons) {
            CheckBox larger_buttons = (CheckBox)findViewById(R.id.larger_buttons);
            larger_buttons.setChecked(true);
        }
        if (OptionStore.no_buttons) {
            CheckBox no_buttons = (CheckBox)findViewById(R.id.no_buttons);
            no_buttons.setChecked(true);
        }
        if (OptionStore.japanese_mode) {
            CheckBox japanese_mode = (CheckBox)findViewById(R.id.japanese_mode);
            japanese_mode.setChecked(true);
        }
        if (OptionStore.no_stretching) {
            CheckBox no_stretching = (CheckBox)findViewById(R.id.no_stretching);
            no_stretching.setChecked(true);
        }
        if (OptionStore.game_genie) {
            CheckBox game_genie = (CheckBox)findViewById(R.id.game_genie);
            game_genie.setChecked(true);
        }

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

    public void applySettings() {
        // define variables
        StringBuilder settings = new StringBuilder();
        CheckBox orientation_lock = (CheckBox)findViewById(R.id.orientation_lock);
        CheckBox disable_sound = (CheckBox)findViewById(R.id.disable_sound);
        CheckBox larger_buttons = (CheckBox)findViewById(R.id.larger_buttons);
        CheckBox no_buttons = (CheckBox)findViewById(R.id.no_buttons);
        CheckBox japanese_mode = (CheckBox)findViewById(R.id.japanese_mode);
        CheckBox no_stretching = (CheckBox)findViewById(R.id.no_stretching);
        CheckBox game_genie = (CheckBox)findViewById(R.id.game_genie);
        boolean errors = false;

        settings.append("orientation_lock=");
        if (orientation_lock.isChecked()) {
            settings.append("1\n");
            if (getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT)
                settings.append("orientation=portrait\n");
            else
                settings.append("orientation=landscape\n");
        } else {
            settings.append("0\n");
        }
        settings.append("disable_sound=");
        if (disable_sound.isChecked())
            settings.append("1\n");
        else
            settings.append("0\n");
        settings.append("larger_buttons=");
        if (larger_buttons.isChecked())
            settings.append("1\n");
        else
            settings.append("0\n");
        settings.append("no_buttons=");
        if (no_buttons.isChecked())
            settings.append("1\n");
        else
            settings.append("0\n");
        settings.append("japanese_mode=");
        if (japanese_mode.isChecked())
            settings.append("1\n");
        else
            settings.append("0\n");
        settings.append("no_stretching=");
        if (no_stretching.isChecked())
            settings.append("1\n");
        else
            settings.append("0\n");
        if (OptionStore.default_path != null) {
            if (OptionStore.default_path.length() > 0) {
                settings.append("default_path=" + OptionStore.default_path + "\n");
            }
        }
        settings.append("game_genie=");
        if (game_genie.isChecked())
            settings.append("1\n");
        else
            settings.append("0\n");


        // define settings file
        File optionsFile = new File(OptionsActivity.this.getFilesDir() + "/settings.ini");

        // set length of file to zero
        RandomAccessFile r = null;
        try {
            r = new RandomAccessFile(optionsFile, "rw");
            r.setLength(0);
        }
        catch (Exception e) {
            Log.e("OptionsActivity", "Problem occurred with RandomAccessFile: " + e);
            errors = true;
        }
        finally {
            // close random acess file
            try {
                r.close();
            }
            catch (IOException e) {
                Log.e("OptionsActivity", "Couldn't close RandomAccessFile: " + e);
                errors = true;
            }
        }

        // write settings to file
        BufferedWriter settingsWriter = null;
        try {
            settingsWriter = new BufferedWriter(new FileWriter(optionsFile));
            settingsWriter.write(settings.toString(), 0, settings.length());
        }
        catch (Exception e) {
            Log.e("OptionsActivity", "Problem occurred writing to settings file: " + e);
            errors = true;
        }
        finally {
            // close settings file
            try {
                settingsWriter.close();
            }
            catch (IOException e) {
                Log.e("OptionsActivity", "Couldn't close BufferedWriter: " + e);
                errors = true;
            }
        }

        Toast status = null;
        if (errors) {
            status = Toast.makeText(OptionsActivity.this, "Failed to update settings file", Toast.LENGTH_SHORT);
            status.show();
        } else {
            status = Toast.makeText(OptionsActivity.this, "Successfully updated settings file", Toast.LENGTH_SHORT);
            OptionStore.updateOptionsFromFile(optionsFile.getAbsolutePath());
            status.show();
            finish();
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
                if (selectionObj != null)
                    selectionObj.activate();
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
            return OptionsActivity.this.motionEvent(event);
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
                cv.unHighlight();
            } else if (event.getAction() == MotionEvent.ACTION_UP) {
                cv.highlight();

                // This is where we apply the settings
                if (v.getId() == R.id.options_apply_button) {
                    applySettings();
                }
            }

            return true;
        }
    }
}
