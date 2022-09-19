/* MasterEmu help screen source code file
   copyright Phil Potter, 2022 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.widget.TextView;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import android.text.method.ScrollingMovementMethod;

/**
 * This class acts as the help screen of the app.
 */
public class HelpActivity extends Activity {

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
     * This method creates the help screen.
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.help_activity);

        StringBuilder helpText = new StringBuilder();
        String line = new String();
        BufferedReader bufferedStream = null;

        try {
            bufferedStream = new BufferedReader(new InputStreamReader(getAssets().open("help.txt")));

            while ((line = bufferedStream.readLine()) != null) {
                helpText.append(line);
                helpText.append('\n');
            }
        }
        catch (IOException e) {
            Log.e("HelpActivity:", "Encountered error reading help file: " + e.toString());
        }
        finally {
            try {
                bufferedStream.close();
            }
            catch (IOException e) {
                Log.e("HelpActivity:", "Encountered error closing help file: " + e.toString());
            }
        }

        TextView helpView = (TextView)findViewById(R.id.help_view);
        helpView.setText(helpText);
        helpView.setMovementMethod(new ScrollingMovementMethod());

        // Set focus
        helpView.requestFocus();
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
            if (keycode == KeyEvent.KEYCODE_BUTTON_A) {
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
}
