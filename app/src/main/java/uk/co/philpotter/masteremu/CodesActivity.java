/* MasterEmu Game Genie/Action Replay code browser source code file
   copyright Phil Potter, 2022 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.os.Bundle;
import android.text.Spanned;
import android.widget.BaseAdapter;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.Toast;
import android.view.View;
import android.view.ViewGroup;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.widget.TextView;
import android.widget.AdapterView;
import android.app.AlertDialog;
import android.widget.RadioButton;
import android.widget.EditText;
import android.widget.RadioGroup;
import android.text.InputFilter;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.BufferedReader;
import java.io.FileWriter;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.util.ArrayList;

import org.libsdl.app.SDLActivity;

/**
 * This class loads cheats and then initiates the SDLActivity in place of FileBrowser.
 */
public class CodesActivity extends Activity {

    // Constant for storage permission request
    private static final int READ_STORAGE_REQUEST = 0;
    private static final int WRITE_STORAGE_REQUEST = 1;

    // Static array for handing over codes to SDL
    public static long[] transferCodes;

    // Define instance variables
    private String checksumStr;
    private ListView lv;
    private CodeListAdapter codeList;
    private CodeListener codeListener;
    private TopListener topListener;
    private View topBar;

    /**
     * This method sets the screen orientation when locked, and loads the actual codes.
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
     * This method saves our codes when stopping the activity.
     */
    @Override
    protected void onStop() {
        super.onStop();

        saveCodeList(checksumStr);
    }

    /**
     * This method creates a list for showing all the current cheats.
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.codes_activity);

        checksumStr = FileBrowser.transferChecksum;

        topBar = findViewById(R.id.codes_topbar_view);

        lv = (ListView)findViewById(R.id.codelist);
        codeList = new CodeListAdapter();

        topListener = new TopListener();
        topBar.setOnClickListener(topListener);
        topBar.setOnLongClickListener(topListener);
        topBar.setOnKeyListener(topListener);
        lv.setAdapter(codeList);
        codeListener = new CodeListener();
        lv.setOnItemClickListener(codeListener);
        lv.setOnItemLongClickListener(codeListener);
        lv.setOnKeyListener(codeListener);

        populateCodeList(checksumStr);

        // Set focus
        lv.requestFocus();
    }

    /**
     * This method finds all saved cheats for the supplied ROM checksum, and displays them.
     */
    private void populateCodeList(String path) {
        // Create directory if it doesn't already exist
        File gameFolder = new File(getFilesDir() + "/" + path);
        if (!gameFolder.exists())
            gameFolder.mkdir();

        // Load file if it exists
        File codesFile = new File(getFilesDir() + "/" + path + "/listof.codes");
        if (!codesFile.exists())
            return;

        // Open file
        BufferedReader codeReader = null;
        try {
            codeReader = new BufferedReader(new FileReader(codesFile));
        }
        catch (FileNotFoundException e) {
            toast("Couldn't open an existing codes file");
            return;
        }

        // Read from file line by line, instantiating as we go
        String tempLine = null;
        try {
            while ((tempLine = codeReader.readLine()) != null) {
                if (tempLine.contains(":")) {
                    codeList.codeList.add(new Code(tempLine));
                }
            }
        }
        catch (IOException e) {
            toast("Couldn't read from an existing codes file");
            return;
        }

        // Close file and refresh listview
        try {
            codeReader.close();
        }
        catch (IOException e) {
            toast("Couldn't close an existing codes file");
            return;
        }
        codeList.notifyDataSetChanged();
    }

    private void saveCodeList(String path) {
        // Create directory if it doesn't already exist
        File gameFolder = new File(getFilesDir() + "/" + path);
        if (!gameFolder.exists())
            gameFolder.mkdir();

        // Load file
        File codesFile = new File(getFilesDir() + "/" + path + "/listof.codes");

        // Set length of file to zero
        RandomAccessFile r = null;
        if (codesFile.exists()) {
            try {
                r = new RandomAccessFile(codesFile, "rw");
                r.setLength(0);
            } catch (Exception e) {
                toast("Couldn't truncate codes file to zero bytes");
                return;
            } finally {
                // Close random acess file
                try {
                    r.close();
                } catch (IOException e) {
                    toast("Couldn't close truncated codes file");
                    return;
                }
            }
        }

        // Open codes file for writing
        BufferedWriter codeWriter = null;
        try {
            codeWriter = new BufferedWriter(new FileWriter(codesFile));
        }
        catch (IOException e) {
            toast("Couldn't open codes file");
            return;
        }

        // Iterate through list, writing line by line
        for (Code code : codeList.codeList) {
            try {
                codeWriter.write(code.generateLineForStorage() + "\n");
            }
            catch (IOException e) {
                toast("Couldn't write code to codes file");
                return;
            }
        }

        // Close codes file
        try {
            codeWriter.close();
        }
        catch (IOException e) {
            toast("Couldn't close codes file");
            return;
        }
    }

    private void toast(String msg) {
        Toast toast = Toast.makeText(CodesActivity.this, msg, Toast.LENGTH_SHORT);
        toast.show();
    }

    private class CodeListener implements AdapterView.OnItemClickListener, AdapterView.OnItemLongClickListener, View.OnKeyListener {
        @Override
        public void onItemClick(AdapterView parent, View view, int position, long id) {
            CodesActivity.this.codeList.codeList.get(position).toggle();
            CodesActivity.this.codeList.notifyDataSetChanged();
        }

        @Override
        public boolean onItemLongClick(AdapterView parent, View view, int position, long id) {
            AlertDialog.Builder ab = new AlertDialog.Builder(CodesActivity.this);
            View av = CodesActivity.this.getLayoutInflater().inflate(R.layout.code_dialogue_edit_delete, null);
            final int innerPosition = position;

            ab.setPositiveButton("Edit", new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    CodesActivity.this.topListener.handleTopBar(true, innerPosition);
                }
            });

            ab.setNeutralButton("Delete", new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    AlertDialog.Builder dab = new AlertDialog.Builder(CodesActivity.this);
                    View dav = CodesActivity.this.getLayoutInflater().inflate(R.layout.code_dialogue_delete, null);

                    dab.setPositiveButton("Yes", new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            CodesActivity.this.codeList.codeList.remove(innerPosition);
                            CodesActivity.this.codeList.notifyDataSetChanged();
                            toast("Code deleted");
                        }
                    });

                    dab.setNegativeButton("Cancel", null);

                    AlertDialog dad = dab.create();

                    dad.setView(dav, 0, 0, 0, 0);
                    dad.show();
                }
            });

            ab.setNegativeButton("Cancel", null);

            AlertDialog ad = ab.create();

            ad.setView(av, 0, 0, 0, 0);
            ad.show();

            return true;
        }

        @Override
        public boolean onKey(View v, int keyCode, KeyEvent event) {
            if (event.getAction() == KeyEvent.ACTION_UP && keyCode == KeyEvent.KEYCODE_BUTTON_X) {
                CodesActivity.this.topListener.handleTopBar(false, -1);
                return true;
            }

            if (event.getAction() == KeyEvent.ACTION_UP && keyCode == KeyEvent.KEYCODE_BUTTON_START) {
                CodesActivity.this.topListener.onClick(v);
                return true;
            }

            if (event.getAction() == KeyEvent.ACTION_UP && keyCode == KeyEvent.KEYCODE_BUTTON_A) {
                Object temp = CodesActivity.this.lv.getSelectedItem();

                if (temp == null)
                    return false;

                int i = 0;
                for (i = 0; i < CodesActivity.this.codeList.codeList.size(); ++i) {
                    if (temp == CodesActivity.this.codeList.getItem(i)) {
                        View tempView = CodesActivity.this.lv.getSelectedView();
                        CodesActivity.this.codeListener.onItemClick(CodesActivity.this.lv, tempView, i, i);
                        break;
                    }
                }
                return true;
            }

            if (event.getAction() == KeyEvent.ACTION_UP && keyCode == KeyEvent.KEYCODE_BUTTON_Y) {
                Object temp = CodesActivity.this.lv.getSelectedItem();

                if (temp == null)
                    return false;

                int i = 0;
                for (i = 0; i < CodesActivity.this.codeList.codeList.size(); ++i) {
                    if (temp == CodesActivity.this.codeList.getItem(i)) {
                        View tempView = CodesActivity.this.lv.getSelectedView();
                        CodesActivity.this.codeListener.onItemLongClick(CodesActivity.this.lv, tempView, i, i);
                        break;
                    }
                }
                return true;
            }

            // This sorts out a weird glitch which triggers onItemLongClick erroneously
            if (event.getAction() == KeyEvent.ACTION_DOWN && (event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_A || event.getKeyCode() == KeyEvent.KEYCODE_DPAD_CENTER)) {
                return true;
            }

            return false;
        }
    }

    private class CodeListAdapter extends BaseAdapter {

        private ArrayList<Code> codeList;

        private CodeListAdapter() {
            codeList = new ArrayList<>();
        }

        @Override
        public boolean areAllItemsEnabled() {
            return true;
        }

        @Override
        public boolean isEnabled(int position) {
            if (position < codeList.size())
                return true;
            throw new ArrayIndexOutOfBoundsException();
        }

        @Override
        public int getCount() {
            return codeList.size();
        }

        @Override
        public Object getItem(int position) {
            return codeList.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public boolean isEmpty() {
            return codeList.isEmpty();
        }

        @Override
        public boolean hasStableIds() {
            return false;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            LayoutInflater li = CodesActivity.this.getLayoutInflater();
            TextView tv = (TextView)li.inflate(R.layout.code_row, parent, false);
            tv.setText(codeList.get(position).toString());
            if (codeList.get(position).isEnabled())
                tv.setBackgroundResource(R.drawable.code_selector_enabled);
            tv.setOnKeyListener(new View.OnKeyListener() {
                @Override
                public boolean onKey(View view, int i, KeyEvent keyEvent) {
                    CodesActivity.this.toast("called from item listener");
                    return true;
                }
            });

            return tv;
        }

        @Override
        public int getItemViewType(int position) {
            return 0;
        }

        @Override
        public int getViewTypeCount() {
            return 1;
        }
    }

    private class Code {
        private CodeType codeType;
        private String codeDescription;
        private String code;
        private boolean on;

        private Code(CodeType codeType, String codeDescription, String code) {
            this.codeType = codeType;
            this.codeDescription = codeDescription;
            this.code = code;
            on = false;
        }

        private Code(String lineFromStorage) {
            String[] fields = lineFromStorage.split(":");

            if (fields.length != 4) {
                CodesActivity.this.toast("Code storage line was corrupted");
                throw new IllegalStateException("Code storage line was corrupted");
            }

            codeType = fields[0].equals("AR") ? CodeType.PRO_ACTION_REPLAY : CodeType.GAME_GENIE;
            codeDescription = new String(fields[1]);
            code = new String(fields[2]);
            on = fields[3].equals("enabled") ? true : false;
        }

        private void enable() {
            on = true;
        }

        private boolean isEnabled() {
            return on;
        }

        private void toggle() {
            on = (on ? false : true);
            if (on)
                toast("Code enabled");
            else
                toast("Code disabled");
        }

        @Override
        public String toString() {
            return (codeType == CodeType.PRO_ACTION_REPLAY ? "AR" : "GG") + ": " + codeDescription;
        }

        private String generateLineForStorage() {
            return (codeType == CodeType.PRO_ACTION_REPLAY ? "AR" : "GG") + ":" + codeDescription +
                    ":" + code + ":" + (on ? "enabled" : "disabled");
        }
    }

    private enum CodeType {
        PRO_ACTION_REPLAY, GAME_GENIE
    }

    private void prepareCodesHandover() {
        // Declare temp arraylist to hold enabled codes
        ArrayList<Code> tempList = new ArrayList<>();

        // Check which codes are enabled
        for (Code code : codeList.codeList) {
            if (code.isEnabled()) {
                tempList.add(code);
            }
        }

        // Initialise static array to hold codes in long form
        CodesActivity.transferCodes = new long[tempList.size()];

        // Go through temp arraylist and convert each code to long form, storing in new array
        for (int i = 0; i < tempList.size(); ++i) {
            Code code = tempList.get(i);

            long convertedCode = 0;
            if (code.codeType == CodeType.PRO_ACTION_REPLAY) {
                // Mark code as being Pro Action Replay type
                convertedCode |= 0x8000000000000000L;

                // Split hex blocks out and convert for merging into the long code
                String[] parts = code.code.split("-");
                long firstPart = Long.parseLong(parts[0], 16);
                long secondPart = Long.parseLong(parts[1], 16);
                firstPart <<= 16;

                // Merge parts into long code
                convertedCode |= firstPart;
                convertedCode |= secondPart;
            } else {
                // Mark code as being Game Genie type
                convertedCode |= 0x4000000000000000L;

                // Split hex blocks out and convert for merging into the long code
                String[] parts = code.code.split("-");
                if (parts.length == 3) {
                    // Mark code as containing the optional third block
                    convertedCode |= 0x2000000000000000L;
                }
                long firstPart = Long.parseLong(parts[0], 16);
                long secondPart = Long.parseLong(parts[1], 16);
                firstPart <<= 24;
                secondPart <<= 12;

                // Merge parts into long code
                convertedCode |= firstPart;
                convertedCode |= secondPart;
                if (parts.length == 3) {
                    long thirdPart = Long.parseLong(parts[2], 16);
                    convertedCode |= thirdPart;
                }
            }

            CodesActivity.transferCodes[i] = convertedCode;
        }
    }

    private class TopListener implements View.OnClickListener,View.OnLongClickListener,View.OnKeyListener {
        @Override
        public void onClick(View v) {
            prepareCodesHandover();
            Intent sdlIntent = new Intent(CodesActivity.this, SDLActivity.class);
            startActivity(sdlIntent);
            finish();
        }

        @Override
        public boolean onLongClick(View v) {
            if (v == CodesActivity.this.topBar) {
                handleTopBar(false, -1);
            }
            return true;
        }

        @Override
        public boolean onKey(View v, int keyCode, KeyEvent event) {
            if (v == CodesActivity.this.topBar && event.getAction() == KeyEvent.ACTION_UP && event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_X) {
                handleTopBar(false, -1);
                return true;
            }

            if (v == CodesActivity.this.topBar && event.getAction() == KeyEvent.ACTION_UP && event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_START) {
                this.onClick(v);
                return true;
            }

            return false;
        }

        private void handleTopBar(boolean edit, int editId) {
            AlertDialog.Builder ab = new AlertDialog.Builder(CodesActivity.this);
            View av = CodesActivity.this.getLayoutInflater().inflate(R.layout.code_dialogue, null);

            final RadioGroup codes_dialogue_button_group = av.findViewById(R.id.codes_dialogue_button_group);
            final RadioButton codes_dialogue_ar_button = av.findViewById(R.id.codes_dialogue_ar_button);
            final RadioButton codes_dialogue_gg_button = av.findViewById(R.id.codes_dialogue_gg_button);
            final EditText codes_dialogue_description_input = av.findViewById(R.id.codes_dialogue_description_input);
            final EditText codes_dialogue_ar_code_input_1 = av.findViewById(R.id.codes_dialogue_ar_code_input_1);
            final EditText codes_dialogue_ar_code_input_2 = av.findViewById(R.id.codes_dialogue_ar_code_input_2);
            final EditText codes_dialogue_gg_code_input_1 = av.findViewById(R.id.codes_dialogue_gg_code_input_1);
            final EditText codes_dialogue_gg_code_input_2 = av.findViewById(R.id.codes_dialogue_gg_code_input_2);
            final EditText codes_dialogue_gg_code_input_3 = av.findViewById(R.id.codes_dialogue_gg_code_input_3);
            final LinearLayout codes_dialogue_ar_container = av.findViewById(R.id.codes_dialogue_ar_container);
            final LinearLayout codes_dialogue_gg_container = av.findViewById(R.id.codes_dialogue_gg_container);
            final boolean innerEdit = edit;
            final int innerEditId = editId;

            codes_dialogue_button_group.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
                @Override
                public void onCheckedChanged(RadioGroup radioGroup, int i) {
                    if (i == -1) {
                        // Unchecked, set again
                        i = R.id.codes_dialogue_ar_button;
                    }

                    if (i == R.id.codes_dialogue_ar_button) {
                        clearCodes();
                        codes_dialogue_ar_container.setVisibility(View.VISIBLE);
                        codes_dialogue_gg_container.setVisibility(View.GONE);
                    } else {
                        clearCodes();
                        codes_dialogue_ar_container.setVisibility(View.GONE);
                        codes_dialogue_gg_container.setVisibility(View.VISIBLE);
                    }
                }

                private void clearCodes() {
                    codes_dialogue_ar_code_input_1.setText("");
                    codes_dialogue_ar_code_input_2.setText("");
                    codes_dialogue_gg_code_input_1.setText("");
                    codes_dialogue_gg_code_input_2.setText("");
                    codes_dialogue_gg_code_input_3.setText("");
                }
            });

            InputFilter[] description_filter = new InputFilter[1];
            description_filter[0] = new InputFilter() {

                private StringBuilder newList = new StringBuilder();

                @Override
                public CharSequence filter(CharSequence source, int start, int end, Spanned dest, int dstart, int dend) {
                    // Remove all ':' characters from string before passing it on
                    newList.setLength(0);
                    for (int i = start; i < end; ++i) {
                        char tempChar = source.charAt(i);
                        if (tempChar != ':') {
                            newList.append(tempChar);
                        }
                    }
                    return (newList.length() > 0 ? newList.toString() : "");
                }
            };
            codes_dialogue_description_input.setFilters(description_filter);

            InputFilter[] ar_filter = new InputFilter[2];
            ar_filter[0] = new InputFilter() {

                private StringBuilder newList = new StringBuilder();

                @Override
                public CharSequence filter(CharSequence source, int start, int end, Spanned dest, int dstart, int dend) {
                    // Make all caps here, and also check it is within range
                    newList.setLength(0);
                    for (int i = start; i < end; ++i) {
                        char tempChar = source.charAt(i);
                        tempChar = Character.toUpperCase(tempChar);
                        if (Character.isLetterOrDigit(tempChar)) {
                            if (Character.isLetter(tempChar)) {
                                if (tempChar >= 'A' && tempChar <= 'F') {
                                    newList.append(tempChar);
                                }
                            } else {
                                newList.append(tempChar);
                            }
                        }
                    }
                    return (newList.length() > 0 ? newList.toString() : "");
                }
            };
            ar_filter[1] = new InputFilter.LengthFilter(4);
            codes_dialogue_ar_code_input_1.setFilters(ar_filter);
            codes_dialogue_ar_code_input_2.setFilters(ar_filter);

            InputFilter[] gg_filter = new InputFilter[2];
            gg_filter[0] = new InputFilter() {

                private StringBuilder newList = new StringBuilder();

                @Override
                public CharSequence filter(CharSequence source, int start, int end, Spanned dest, int dstart, int dend) {
                    // Make all caps here, and also check it is within range
                    newList.setLength(0);
                    for (int i = start; i < end; ++i) {
                        char tempChar = source.charAt(i);
                        tempChar = Character.toUpperCase(tempChar);
                        if (Character.isLetterOrDigit(tempChar)) {
                            if (Character.isLetter(tempChar)) {
                                if (tempChar >= 'A' && tempChar <= 'F') {
                                    newList.append(tempChar);
                                }
                            } else {
                                newList.append(tempChar);
                            }
                        }
                    }
                    return (newList.length() > 0 ? newList.toString() : "");
                }
            };
            gg_filter[1] = new InputFilter.LengthFilter(3);
            codes_dialogue_gg_code_input_1.setFilters(gg_filter);
            codes_dialogue_gg_code_input_2.setFilters(gg_filter);
            codes_dialogue_gg_code_input_3.setFilters(gg_filter);

            Code tempCode = null;
            if (innerEdit) {
                tempCode = CodesActivity.this.codeList.codeList.get(innerEditId);

                if (tempCode.codeType == CodeType.PRO_ACTION_REPLAY) {
                    codes_dialogue_ar_button.toggle();
                    String[] codeChunks = tempCode.code.split("-");
                    codes_dialogue_ar_code_input_1.setText(codeChunks[0]);
                    codes_dialogue_ar_code_input_2.setText(codeChunks[1]);
                } else {
                    codes_dialogue_gg_button.toggle();
                    String[] codeChunks = tempCode.code.split("-");
                    codes_dialogue_gg_code_input_1.setText(codeChunks[0]);
                    codes_dialogue_gg_code_input_2.setText(codeChunks[1]);
                    if (codeChunks.length == 3)
                        codes_dialogue_gg_code_input_3.setText(codeChunks[2]);
                }

                codes_dialogue_description_input.setText(tempCode.codeDescription);
            }

            ab.setPositiveButton("Save", null);
            ab.setNegativeButton("Cancel", null);
            final AlertDialog ad = ab.create();
            ad.setView(av, 0, 0, 0, 0);
            ad.show();

            ad.getButton(DialogInterface.BUTTON_POSITIVE).setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View view) {
                    // Add code in here if properly formed

                    // Check description
                    if (codes_dialogue_description_input.getText().length() == 0) {
                        CodesActivity.this.toast("Description cannot be empty");
                        return;
                    }

                    // Check code type
                    if (codes_dialogue_ar_button.isChecked()) {
                        // Check Action Replay format
                        String part1 = codes_dialogue_ar_code_input_1.getText().toString();
                        String part2 = codes_dialogue_ar_code_input_2.getText().toString();
                        if (part1.length() != 4) {
                            CodesActivity.this.toast("1st part of Action Replay code must be four characters");
                            return;
                        }
                        if (part2.length() != 4) {
                            CodesActivity.this.toast("2nd part of Action Replay code must be four characters");
                            return;
                        }

                        // Instantiate code and add to list in memory
                        Code tempCode = new Code(CodeType.PRO_ACTION_REPLAY, codes_dialogue_description_input.getText().toString(),
                                part1 + "-" + part2);
                        if (innerEdit) {
                            CodesActivity.this.codeList.codeList.set(innerEditId, tempCode);
                            CodesActivity.this.toast("Pro Action Replay code successfully updated");
                        } else {
                            CodesActivity.this.codeList.codeList.add(tempCode);
                            CodesActivity.this.toast("Pro Action Replay code successfully added");
                        }
                        CodesActivity.this.codeList.notifyDataSetChanged();

                        ad.dismiss();
                    }
                    else if (codes_dialogue_gg_button.isChecked()) {
                        // Check Game Genie format
                        String part1 = codes_dialogue_gg_code_input_1.getText().toString();
                        String part2 = codes_dialogue_gg_code_input_2.getText().toString();
                        String part3 = codes_dialogue_gg_code_input_3.getText().toString();
                        if (part1.length() != 3) {
                            CodesActivity.this.toast("1st part of Game Genie code must be three characters");
                            return;
                        }
                        if (part2.length() != 3) {
                            CodesActivity.this.toast("2nd part of Game Genie code must be three characters");
                            return;
                        }
                        if (part3.length() != 3 && part3.length() != 0) {
                            CodesActivity.this.toast("3rd part of Game Genie code must be three characters (if present)");
                            return;
                        }

                        // Instantiate code and add to list in memory
                        Code tempCode = new Code(CodeType.GAME_GENIE, codes_dialogue_description_input.getText().toString(),
                                part1 + "-" + part2 + (part3.length() != 0 ? ("-" + part3) : ""));
                        if (innerEdit) {
                            CodesActivity.this.codeList.codeList.set(innerEditId, tempCode);
                            CodesActivity.this.toast("Game Genie code successfully updated");
                        } else {
                            CodesActivity.this.codeList.codeList.add(tempCode);
                            CodesActivity.this.toast("Game Genie code successfully added");
                        }
                        CodesActivity.this.codeList.notifyDataSetChanged();

                        ad.dismiss();
                    }
                    else {
                        CodesActivity.this.toast("Neither code type is selected");
                        return;
                    }
                }
            });
        }
    }
}
