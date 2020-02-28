/* MasterEmu titlescreen source code file
   copyright Phil Potter, 2019 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.widget.LinearLayout.LayoutParams;
import android.view.KeyEvent;
import android.view.InputDevice;
import java.io.File;
import java.io.IOException;
import android.util.Log;

/**
 * This class acts as the titlescreen of the app.
 */
public class TitlescreenActivity extends Activity {

    // define instance variables
    private ControllerSelection selectionObj;
    private long timeSinceLastAnaloguePress = 0;

    /**
     * This loads settings when the app first starts.
     */
    @Override
    protected void onStart() {
        super.onStart();
        File f = new File(getFilesDir() + "/default_file");
        try {
            f.createNewFile();
        }
        catch (IOException e) {
            Log.e("TitlescreenActivity", "Couldn't create default_file: " + e);
        }
        OptionStore.updateOptionsFromFile(getFilesDir() + "/settings.ini");
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
     * This kills the app for us when back is pressed.
     */
    @Override
    public void onBackPressed() {
        finish();
    }

    /**
     * This method creates the titlescreen.
     */
    @SuppressWarnings("deprecation")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        try {
            Runtime.getRuntime().loadLibrary("SDL2");
        }
        catch (UnsatisfiedLinkError e) {
            Intent wrongCpu = new Intent(this, WrongCpuActivity.class);
            startActivity(wrongCpu);
            finish();
        }
        setContentView(R.layout.titlescreen_activity);

        // Add listeners
        ButtonColourListener bcl = new ButtonColourListener();
        ControllerTextView title_load_button = (ControllerTextView)findViewById(R.id.title_load_button);
        ControllerTextView title_options_button = (ControllerTextView)findViewById(R.id.title_options_button);
        ControllerTextView title_help_button = (ControllerTextView)findViewById(R.id.title_help_button);
        ControllerTextView title_credits_button = (ControllerTextView)findViewById(R.id.title_credits_button);
        ControllerTextView title_extras_button = (ControllerTextView)findViewById(R.id.title_extras_button);
        ControllerTextView title_manage_states_button = (ControllerTextView)findViewById(R.id.title_manage_states_button);
        ControllerTextView title_exit_button = (ControllerTextView)findViewById(R.id.title_exit_button);
        title_load_button.setOnTouchListener(bcl);
        title_options_button.setOnTouchListener(bcl);
        title_help_button.setOnTouchListener(bcl);
        title_credits_button.setOnTouchListener(bcl);
        title_extras_button.setOnTouchListener(bcl);
        title_manage_states_button.setOnTouchListener(bcl);
        title_exit_button.setOnTouchListener(bcl);

        // Change visiblity
        findViewById(R.id.titlescreen_wait).setVisibility(View.GONE);
        findViewById(R.id.titlescreen_go).setVisibility(View.VISIBLE);

        // Set all buttons to same width
        int longest = 0;
        title_load_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        title_options_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        title_help_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        title_credits_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        title_extras_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        title_manage_states_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        title_exit_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        if (title_load_button.getMeasuredWidth() > longest)
            longest = title_load_button.getMeasuredWidth();
        if (title_options_button.getMeasuredWidth() > longest)
            longest = title_options_button.getMeasuredWidth();
        if (title_help_button.getMeasuredWidth() > longest)
            longest = title_help_button.getMeasuredWidth();
        if (title_credits_button.getMeasuredWidth() > longest)
            longest = title_credits_button.getMeasuredWidth();
        if (title_extras_button.getMeasuredWidth() > longest)
            longest = title_extras_button.getMeasuredWidth();
        if (title_manage_states_button.getMeasuredWidth() > longest)
            longest = title_manage_states_button.getMeasuredWidth();
        if (title_exit_button.getMeasuredWidth() > longest)
            longest = title_exit_button.getMeasuredWidth();
        title_load_button.setLayoutParams(new LayoutParams(longest, title_load_button.getMeasuredHeight()));
        title_options_button.setLayoutParams(new LayoutParams(longest, title_options_button.getMeasuredHeight()));
        title_help_button.setLayoutParams(new LayoutParams(longest, title_help_button.getMeasuredHeight()));
        title_credits_button.setLayoutParams(new LayoutParams(longest, title_credits_button.getMeasuredHeight()));
        title_extras_button.setLayoutParams(new LayoutParams(longest, title_extras_button.getMeasuredHeight()));
        title_manage_states_button.setLayoutParams(new LayoutParams(longest, title_manage_states_button.getMeasuredHeight()));
        title_exit_button.setLayoutParams(new LayoutParams(longest, title_exit_button.getMeasuredHeight()));

        // Set drawables on buttons and create their intents
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
        if (android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP_MR1) {
            dark = getResources().getDrawable(R.drawable.view_greyed_out_border);
            light = getResources().getDrawable(R.drawable.view_border);
        } else {
            dark = getResources().getDrawable(R.drawable.view_greyed_out_border, null);
            light = getResources().getDrawable(R.drawable.view_border, null);
        }
        title_load_button.setActiveDrawable(dark);
        title_load_button.setInactiveDrawable(light);
        title_load_button.setHighlightedTextColour(darkText);
        title_load_button.setUnhighlightedTextColour(lightText);
        title_options_button.setActiveDrawable(dark);
        title_options_button.setInactiveDrawable(light);
        title_options_button.setHighlightedTextColour(darkText);
        title_options_button.setUnhighlightedTextColour(lightText);
        title_help_button.setActiveDrawable(dark);
        title_help_button.setInactiveDrawable(light);
        title_help_button.setHighlightedTextColour(darkText);
        title_help_button.setUnhighlightedTextColour(lightText);
        title_credits_button.setActiveDrawable(dark);
        title_credits_button.setInactiveDrawable(light);
        title_credits_button.setHighlightedTextColour(darkText);
        title_credits_button.setUnhighlightedTextColour(lightText);
        title_extras_button.setActiveDrawable(dark);
        title_extras_button.setInactiveDrawable(light);
        title_extras_button.setHighlightedTextColour(darkText);
        title_extras_button.setUnhighlightedTextColour(lightText);
        title_manage_states_button.setActiveDrawable(dark);
        title_manage_states_button.setInactiveDrawable(light);
        title_manage_states_button.setHighlightedTextColour(darkText);
        title_manage_states_button.setUnhighlightedTextColour(lightText);
        title_exit_button.setActiveDrawable(dark);
        title_exit_button.setInactiveDrawable(light);
        title_exit_button.setHighlightedTextColour(darkText);
        title_exit_button.setUnhighlightedTextColour(lightText);
        Intent titleIntent = new Intent(TitlescreenActivity.this, FileBrowser.class);
        titleIntent.putExtra("actionType", "load_rom");
        title_load_button.setIntent(titleIntent);
        title_options_button.setIntent(new Intent(TitlescreenActivity.this, OptionsActivity.class));
        title_help_button.setIntent(new Intent(TitlescreenActivity.this, HelpActivity.class));
        title_credits_button.setIntent(new Intent(TitlescreenActivity.this, CreditsActivity.class));
        title_extras_button.setIntent(new Intent(TitlescreenActivity.this, ExtrasActivity.class));
        title_manage_states_button.setIntent(new Intent(TitlescreenActivity.this, ManageStatesActivity.class));

        // Create selection objects
        selectionObj = new ControllerSelection();
        selectionObj.addMapping(title_load_button);
        selectionObj.addMapping(title_options_button);
        selectionObj.addMapping(title_help_button);
        selectionObj.addMapping(title_credits_button);
        selectionObj.addMapping(title_extras_button);
        selectionObj.addMapping(title_manage_states_button);
        selectionObj.addMapping(title_exit_button);

        // Set focus
        title_load_button.requestFocus();

        // Set new MasterEmuMotionListener
        View titlescreen_root = findViewById(R.id.titlescreen_root);
        titlescreen_root.setOnGenericMotionListener(new MasterEmuMotionListener());
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
                if (selectionObj != null)
                    selectionObj.activate();
                returnVal = true;
            } else if (keycode == KeyEvent.KEYCODE_BUTTON_B) {
                if (selectionObj != null)
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
            return TitlescreenActivity.this.motionEvent(event);
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
            ControllerTextView cv = (ControllerTextView)v;
            if (event.getAction() == MotionEvent.ACTION_DOWN) {
                cv.highlight();
            } else if (event.getAction() == MotionEvent.ACTION_UP) {
                cv.unHighlight();
                cv.activate();
            }

            return true;
        }
    }

}
