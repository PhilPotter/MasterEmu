/* MasterEmu extras screen source code file
   copyright Phil Potter, 2019 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
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
import android.net.Uri;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

/**
 * This class acts as the extras screen of the app.
 */
public class ExtrasActivity extends Activity {

    // define instance variables
    private ControllerSelection selectionObj;
    private long timeSinceLastAnaloguePress = 0;

    /**
     * This method creates the extras screen.
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.extras_activity);

        StringBuilder extrasText = new StringBuilder();
        String line = new String();
        BufferedReader bufferedStream = null;

        try {
            bufferedStream = new BufferedReader(new InputStreamReader(getAssets().open("extras.txt")));

            while ((line = bufferedStream.readLine()) != null) {
                extrasText.append(line);
                extrasText.append('\n');
            }
        }
        catch (IOException e) {
            Log.e("ExtrasActivity:", "Encountered error reading extras file: " + e.toString());
        }
        finally {
            try {
                bufferedStream.close();
            }
            catch (IOException e) {
                Log.e("ExtrasActivity:", "Encountered error closing extras file: " + e.toString());
            }
        }

        TextView extrasView = (TextView)findViewById(R.id.extras_blurb);
        extrasView.setText(extrasText);

        ButtonColourListener bcl = new ButtonColourListener();
        ControllerTextView extras_website_button = (ControllerTextView)findViewById(R.id.extras_website_button);
        extras_website_button.setOnTouchListener(bcl);
        ControllerTextView extras_source_button = (ControllerTextView)findViewById(R.id.extras_source_button);
        extras_source_button.setOnTouchListener(bcl);

        // Set all buttons to same width
        int longest = 0;
        extras_website_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        extras_source_button.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        if (extras_website_button.getMeasuredWidth() > longest)
            longest = extras_website_button.getMeasuredWidth();
        if (extras_source_button.getMeasuredWidth() > longest)
            longest = extras_source_button.getMeasuredWidth();

        extras_website_button.setLayoutParams(new LinearLayout.LayoutParams(longest, extras_website_button.getMeasuredHeight()));
        extras_source_button.setLayoutParams(new LinearLayout.LayoutParams(longest, extras_website_button.getMeasuredHeight()));

        // Load drawables for donate button
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
        extras_website_button.setActiveDrawable(dark);
        extras_website_button.setInactiveDrawable(light);
        extras_website_button.setHighlightedTextColour(darkText);
        extras_website_button.setUnhighlightedTextColour(lightText);
        extras_source_button.setActiveDrawable(dark);
        extras_source_button.setInactiveDrawable(light);
        extras_source_button.setHighlightedTextColour(darkText);
        extras_source_button.setUnhighlightedTextColour(lightText);

        // Setup button intents and make sure to add extra info to do the right thing
        Intent pottersoft_intent = new Intent(Intent.ACTION_VIEW);
        String pottersoft_url = getResources().getString(R.string.pottersoft_url);
        pottersoft_intent.setData(Uri.parse(pottersoft_url));
        extras_website_button.setIntent(pottersoft_intent);
        Intent source_intent = new Intent(Intent.ACTION_VIEW);
        String source_url = getResources().getString(R.string.github_url);
        source_intent.setData(Uri.parse(source_url));
        extras_source_button.setIntent(source_intent);

        // Create selection object and add mappings to it.
        selectionObj = new ControllerSelection();
        selectionObj.addMapping(extras_website_button);
        selectionObj.addMapping(extras_source_button);

        // Set focus
        View extras_title = findViewById(R.id.extras_title);
        extras_title.requestFocus();

        // Set new MasterEmuMotionListener
        View extras_root = findViewById(R.id.extras_root);
        extras_root.setOnGenericMotionListener(new MasterEmuMotionListener());
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
            return ExtrasActivity.this.motionEvent(event);
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
                cv.activate();
            }

            return true;
        }
    }
}
