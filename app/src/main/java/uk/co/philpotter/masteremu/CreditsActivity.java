/* MasterEmu credits screen source code file
   copyright Phil Potter, 2022 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import java.io.InputStreamReader;
import java.io.BufferedReader;
import android.util.Log;
import java.io.IOException;
import android.widget.TextView;
import android.text.method.ScrollingMovementMethod;
import android.view.KeyEvent;
import android.content.Intent;

/**
 * This class acts as the credits screen of the app.
 */
public class CreditsActivity extends Activity {

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
     * This method creates the credits screen.
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.credits_activity);

        StringBuilder creditsText = new StringBuilder();
        String line = new String();
        BufferedReader bufferedStream = null;

        try {
            bufferedStream = new BufferedReader(new InputStreamReader(getAssets().open("credits.txt")));

            while ((line = bufferedStream.readLine()) != null) {
                creditsText.append(line);
                creditsText.append('\n');
            }
        }
        catch (IOException e) {
            Log.e("CreditsActivity:", "Encountered error reading credits file: " + e.toString());
        }
        finally {
            try {
                bufferedStream.close();
            }
            catch (IOException e) {
                Log.e("CreditsActivity:", "Encountered error closing credits file: " + e.toString());
            }
        }

        TextView creditsView = (TextView)findViewById(R.id.credits_view);
        creditsView.setText(creditsText);
        creditsView.setMovementMethod(new ScrollingMovementMethod());

        // Set focus
        creditsView.requestFocus();
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
