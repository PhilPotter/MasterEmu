/* MasterEmu file browser source code file
   copyright Phil Potter, 2019 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.pm.ActivityInfo;
import android.os.Build;
import android.os.Bundle;

import java.io.BufferedReader;
import java.io.File;
import android.os.Environment;

import android.view.KeyEvent;
import android.widget.BaseAdapter;
import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import android.view.LayoutInflater;
import android.widget.LinearLayout;
import android.widget.EditText;
import android.text.TextWatcher;
import android.text.Editable;

import java.io.FileNotFoundException;
import java.io.FileReader;
import java.util.ArrayList;
import android.content.Intent;
import android.widget.AdapterView;
import java.io.IOException;
import android.util.Log;

import java.util.Calendar;
import java.util.Collections;
import android.graphics.drawable.Drawable;
import android.widget.Toast;
import java.io.RandomAccessFile;
import java.io.BufferedWriter;
import java.io.FileWriter;
import java.util.Date;
import java.util.zip.ZipFile;
import java.util.zip.ZipEntry;
import java.util.zip.CRC32;
import java.util.Enumeration;

import android.Manifest;
import androidx.core.content.ContextCompat;
import android.content.pm.PackageManager;
import androidx.core.app.ActivityCompat;

/**
 * This class implements a file browser to allow the user
 * to select a ROM path, state ZIP file or export directory to
 * pass to the native code or StateIO routines.
 */
public class FileBrowser extends Activity {

    // constant for storage permission request
    private static final int FILEBROWSER_READ_STORAGE_REQUEST = 0;
    private static final int FILEBROWSER_WRITE_STORAGE_REQUEST = 1;

    // static variable for passing RomData object to SDL, and checksum to codes activity
    public static RomData transferData = null;
    public static String transferChecksum = null;

    // define instance variables
    private String internalStorageRoot = System.getenv("EXTERNAL_STORAGE");
    private String externalStorageRoot = System.getenv("SECONDARY_STORAGE");
    private String defaultFilePath;
    private String thisDirPath;
    private String importPath;
    private String currentPath = "";
    private String pathToLoad;
    private ControllerSelection selectionObj;
    private StorageClickListener sc;
    private PathClickListener pc;
    private String actionType;
    private volatile boolean romClicked;
    private boolean firstPopulation;

    // variable to reference EmuGridView instance
    private EmuGridView g;

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
     * This method creates the layout (header + file grid).
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        firstPopulation = true;
        romClicked = false;
        actionType = getIntent().getStringExtra("actionType");
        setContentView(R.layout.filebrowser_activity);
        if (actionType.equals("import_states")) {
            TextView managestatesView = (TextView) findViewById(R.id.filebrowser_header);
            managestatesView.setText(getResources().getString(R.string.filebrowser_header_import_states));
        }
        else if (actionType.equals("export_states")) {
            TextView managestatesView = (TextView) findViewById(R.id.filebrowser_header);
            managestatesView.setText(getResources().getString(R.string.filebrowser_header_export_states));
        }
        else {
            LinearLayout searchArea = (LinearLayout)findViewById(R.id.search_area);
            searchArea.setVisibility(View.VISIBLE);
            EditText searchText = (EditText)findViewById(R.id.search_text);
            searchText.addTextChangedListener(new TextWatcher() {
                @Override
                public void afterTextChanged(Editable s) {
                    populateFileGrid(currentPath);
                }

                @Override
                public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

                @Override
                public void onTextChanged(CharSequence s, int start, int count, int after) {}
            });
        }
        defaultFilePath = getFilesDir() + "/default_file";
        thisDirPath = getFilesDir() + "/this_dir";
        ControllerTextView internal = (ControllerTextView)findViewById(R.id.internal_view);
        ControllerTextView external = (ControllerTextView)findViewById(R.id.external_view);
        g = (EmuGridView)findViewById(R.id.filegrid);
        pc = new PathClickListener();
        g.setOnItemClickListener(pc);

        // Setup button drawables
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
            light = getResources().getDrawable(R.drawable.view_border);
            dark = getResources().getDrawable(R.drawable.view_greyed_out_border);
        } else {
            light = getResources().getDrawable(R.drawable.view_border, null);
            dark = getResources().getDrawable(R.drawable.view_greyed_out_border, null);
        }
        internal.setActiveDrawable(light);
        internal.setInactiveDrawable(dark);
        internal.setHighlightedTextColour(lightText);
        internal.setUnhighlightedTextColour(darkText);
        external.setActiveDrawable(light);
        external.setInactiveDrawable(dark);
        external.setHighlightedTextColour(lightText);
        external.setUnhighlightedTextColour(darkText);
        sc = new StorageClickListener();
        internal.setOnClickListener(sc);
        external.setOnClickListener(sc);

        if (externalStorageRoot != null) {
            if (externalStorageRoot.contains(":")) {
                String[] temp = externalStorageRoot.split(":");
                for (int i = 0; i < temp.length; ++i) {
                    if (temp[i].contains("sdcard") || temp[i].contains("sda") || temp[i].contains("external_sd") || temp[i].contains("extSdCard")) {
                        externalStorageRoot = temp[i];
                        break;
                    }
                }
            } else if (externalStorageRoot.contains("/mnt/extSdCard")) {
                externalStorageRoot = "/mnt/extSdCard";
            } else {
                externalStorageRoot = "/storage";
            }
        } else {
            File sda1 = new File("/storage/sda1");
            File external_sd = new File("/mnt/external_sd");
            File extSdCard = new File("/mnt/extSdCard");
            if (sda1.exists() && sda1.isDirectory())
                externalStorageRoot = "/storage/sda1";
            else if (external_sd.exists() && external_sd.isDirectory())
                externalStorageRoot = "/mnt/external_sd";
            else if (extSdCard.exists() && extSdCard.isDirectory())
                externalStorageRoot = "/mnt/extSdCard";
            else
                externalStorageRoot = "/storage";
        }

        // this is a better way of finding the primary storage area
        internalStorageRoot = Environment.getExternalStorageDirectory().getAbsolutePath();

        if (savedInstanceState == null) {
            File defaultPath = null;
            if (OptionStore.default_path != null && OptionStore.default_path.length() > 0 && actionType.equals("load_rom")) {
                defaultPath = new File(OptionStore.default_path);
            }
            if (defaultPath != null && defaultPath.exists() && defaultPath.isDirectory()) {
                if (externalStorageRoot != null && OptionStore.default_path.contains(externalStorageRoot) && !OptionStore.default_path.contains(internalStorageRoot)) {
                    internal.unHighlight();
                    external.highlight();
                }
                //populateFileGrid(OptionStore.default_path);
                pathToLoad = OptionStore.default_path;
            } else {
                if (internalStorageRoot == null)
                    internalStorageRoot = Environment.getExternalStorageDirectory().getAbsolutePath();
                //populateFileGrid(internalStorageRoot);
                pathToLoad = internalStorageRoot;
            }
            if (externalStorageRoot != null) {
                File externalTemp = new File(externalStorageRoot);
                if (!(externalTemp.exists() && externalTemp.canRead())) {
                    externalStorageRoot = null;
                }
            }
        } else {
            currentPath = savedInstanceState.getString("CURRENT_PATH");
            //populateFileGrid(currentPath);
            pathToLoad = currentPath;
            String internalOrExternal = savedInstanceState.getString("STORAGE_TYPE");
            if (internalOrExternal.equals("EXTERNAL")) {
                internal.unHighlight();
                external.highlight();
            }
        }

        // Setup internal and external button controller handling
        selectionObj = new ControllerSelection();
        selectionObj.isFileBrowser();
        selectionObj.addMapping(internal);
        if (externalStorageRoot != null)
            selectionObj.addMapping(external);

        // Set focus
        internal.requestFocus();

        // Check permission for storage and request if not present (when running on Android 6.0 or above)
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.M) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE)
                    != PackageManager.PERMISSION_GRANTED) { // not granted
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.READ_EXTERNAL_STORAGE},
                        FILEBROWSER_READ_STORAGE_REQUEST);
            } else {
                populateFileGrid(pathToLoad);
            }
        } else {
            populateFileGrid(pathToLoad);
        }
    }

    /**
     * This callback runs after a permissions request.
     * @param requestCode
     * @param permissions
     * @param grantResults
     */
    @Override
    public void onRequestPermissionsResult (int requestCode, String[] permissions, int[] grantResults) {
        switch (requestCode) {
            case FILEBROWSER_READ_STORAGE_REQUEST:
                if (grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    // permission granted, load grid as normal
                    populateFileGrid(pathToLoad);
                } else {
                    // permission denied
                    finish();
                }
                break;
            case FILEBROWSER_WRITE_STORAGE_REQUEST:
                if (grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    // permission granted, load grid as normal
                    exportMethod();
                } else {
                    // permission denied
                    finish();
                }
                break;
        }
    }

    /**
     * This method saves the path and allows us to handle screen
     * reorientations properly.
     */
    @Override
    public void onSaveInstanceState(Bundle savedInstanceState) {
        // Save current path
        savedInstanceState.putString("CURRENT_PATH", currentPath);
        if (currentPath.contains(internalStorageRoot))
            savedInstanceState.putString("STORAGE_TYPE", "INTERNAL");
        else
            savedInstanceState.putString("STORAGE_TYPE", "EXTERNAL");
    }

    /**
     * This method finds all files and folders in the supplied path
     * and populates the grid with them.
     */
    public void populateFileGrid(String path) {

        // set things up if we are in rom loader mode
        String toCompareAgainst = null;
        if (actionType.equals("load_rom")) {
            toCompareAgainst = ((EditText)findViewById(R.id.search_text)).getText().toString();
        }

        // store eventual entries
        ArrayList<EmuFile> tempList = new ArrayList<>();

        // detect zip file or normal folder
        EmuFile f = new EmuFile(path);
        if (path.toLowerCase().endsWith(".zip") && !f.isDirectory() && actionType.equals("load_rom")) {
            ZipFile zf = null;

            // open zip file
            try {
                zf = new ZipFile(f);
                currentPath = f.getCanonicalPath();
            }
            catch (IOException e) {
                Toast error = Toast.makeText(FileBrowser.this, "Can't open zip file, possibly corrupted", Toast.LENGTH_SHORT);
                error.show();
                return;
            }

            // iterate through its entries, searching for ROM files
            Enumeration zipEntries = zf.entries();
            while (zipEntries.hasMoreElements()) {
                ZipEntry ze = (ZipEntry)zipEntries.nextElement();
                String name = ze.getName().toLowerCase();
                String nameToCompareAgainst = name;
                if (name.contains("/")) {
                    String[] parts = name.split("/");
                    nameToCompareAgainst = parts[parts.length - 1];
                }
                boolean searchStrNotEmpty = !toCompareAgainst.isEmpty();
                boolean fileMatches = nameToCompareAgainst.contains(toCompareAgainst.toLowerCase().trim());
                if (!ze.isDirectory() && (name.endsWith(".gg") || name.endsWith(".sms"))) {
                    if (!searchStrNotEmpty)
                        tempList.add(new EmuFile(ze, zf));
                    else if (fileMatches)
                        tempList.add(new EmuFile(ze, zf));
                }
            }

            // sort entries
            Collections.sort(tempList);
            EmuFile parent = new EmuFile(f.getParent());
            parent.setParent();
            tempList.add(0, parent);

        } else {

            File[] tempFileArray = f.listFiles();
            EmuFile[] tempEmuFileArray = null;
            try {
                tempEmuFileArray = new EmuFile[tempFileArray.length];
            } catch (NullPointerException e) {
                Toast error = Toast.makeText(FileBrowser.this, "Can't open this, probably not mounted", Toast.LENGTH_SHORT);
                error.show();
                return;
            }
            for (int i = 0; i < tempFileArray.length; ++i) {
                try {
                    tempEmuFileArray[i] = new EmuFile(tempFileArray[i].getCanonicalPath());
                } catch (IOException e) {
                    Log.e("populateFileGrid:", "Couldn't get canonical path");
                    System.exit(0);
                }
            }

            String pathCanon = null;
            boolean topLevel = true;
            try {
                currentPath = f.getCanonicalPath();
                if (currentPath.equals((new File(internalStorageRoot)).getCanonicalPath()))
                    pathCanon = (new File(internalStorageRoot)).getCanonicalPath();
                else if (externalStorageRoot != null && currentPath.equals((new File(externalStorageRoot)).getCanonicalPath()))
                    pathCanon = (new File(externalStorageRoot)).getCanonicalPath();

                if (!(currentPath.equals(pathCanon))) {
                    topLevel = false;
                }
            } catch (IOException e) {
                Log.e("populateFileGrid:", "Couldn't get canonical path");
                System.exit(0);
            }

            // add in generic 'set as default' and 'Export Here' item
            EmuFile setDefault = new EmuFile(defaultFilePath);
            EmuFile exportHere = new EmuFile(thisDirPath);

            if (actionType.equals("load_rom")) {
                for (int i = 0; i < tempEmuFileArray.length; ++i) {
                    String fileName = tempEmuFileArray[i].getName().toLowerCase();
                    boolean searchStrNotEmpty = !toCompareAgainst.isEmpty();
                    boolean fileMatches = fileName.contains(toCompareAgainst.toLowerCase().trim());
                    if ((tempEmuFileArray[i].isDirectory() &&
                            !(fileName.charAt(0) == '.')) ||
                            fileName.endsWith(".sms") ||
                            fileName.endsWith(".gg") ||
                            fileName.endsWith(".zip")) {
                        if (tempEmuFileArray[i].isDirectory() || !searchStrNotEmpty) {
                            tempList.add(tempEmuFileArray[i]);
                        } else if (fileMatches) {
                            tempList.add(tempEmuFileArray[i]);
                        }
                    }
                }
            }
            else if (actionType.equals("import_states")) {
                for (int i = 0; i < tempEmuFileArray.length; ++i) {
                    String fileName = tempEmuFileArray[i].getName().toLowerCase();
                    if ((tempEmuFileArray[i].isDirectory() &&
                            !(fileName.charAt(0) == '.')) ||
                            fileName.endsWith(".zip")) {
                        tempList.add(tempEmuFileArray[i]);
                    }
                }
            }
            else if (actionType.equals("export_states")) {
                for (int i = 0; i < tempEmuFileArray.length; ++i) {
                    String fileName = tempEmuFileArray[i].getName().toLowerCase();
                    if ((tempEmuFileArray[i].isDirectory() &&
                            !(fileName.charAt(0) == '.'))) {
                        tempList.add(tempEmuFileArray[i]);
                    }
                }
            }


            Collections.sort(tempList);
            try {
                if (actionType.equals("load_rom")) {
                    if (topLevel) {
                        tempList.add(0, setDefault);
                    } else {
                        tempList.add(0, new EmuFile(f.getCanonicalPath() + "/.."));
                        tempList.add(1, setDefault);
                    }
                }
                else if (actionType.equals("import_states")) {
                    if (!topLevel) {
                        tempList.add(0, new EmuFile(f.getCanonicalPath() + "/.."));
                    }
                }
                else if (actionType.equals("export_states")) {
                    if (topLevel) {
                        tempList.add(0, exportHere);
                    } else {
                        tempList.add(0, new EmuFile(f.getCanonicalPath() + "/.."));
                        tempList.add(1, exportHere);
                    }
                }
            } catch (IOException e) {
                Log.e("populateFileGrid:", "Couldn't get canonical path");
                System.exit(0);
            }
        }

        g = (EmuGridView)findViewById(R.id.filegrid);
        g.setAdapter(new FilesystemAdapter(this, tempList.toArray()));
        g.post(new Runnable() {
            /**
             * This allows the heights to be set after the layout phase has
             * completed, thus preventing errors spewing in logcat.
             */
            @Override
            public void run() {
                // get number of columns
                int itemsInRow = g.getNumColumns();

                // get total number of views
                int totalItems = g.getChildCount();

                // calculate number of rows
                int numOfRows = totalItems / itemsInRow;
                if (totalItems % itemsInRow > 0)
                    ++numOfRows;

                // iterate through rows, changing heights as we go
                for (int row = 0; row < numOfRows; ++row) {
                    int highest = 0;
                    ArrayList<View> rowViews = new ArrayList<View>();

                    for (int item = row * itemsInRow; item < (row * itemsInRow) + itemsInRow; ++item) {

                        if (g.getChildAt(item) != null) {
                            rowViews.add(g.getChildAt(item));
                            int currentHeight = g.getChildAt(item).getMeasuredHeight();
                            highest = (currentHeight > highest) ? currentHeight : highest;
                        }
                    }

                    for (View v : rowViews) {
                        v.setLayoutParams(new EmuGridView.LayoutParams(v.getMeasuredWidth(), highest));
                    }
                }

                g.invalidate();
            }
        });
        if (firstPopulation) {
            g.requestFocus();
            firstPopulation = false;
        }
    }

    /**
     * This class allows us to present a list of File objects as the
     * backing store of a GridView.
     */
    protected class FilesystemAdapter extends BaseAdapter {

        private Object[] filesAndFolders;
        private Context context;
        private LayoutInflater inflater;

        public FilesystemAdapter(Context c, Object[] filesAndFolders) {
            this.context = c;
            this.filesAndFolders = filesAndFolders;
            this.inflater = LayoutInflater.from(c);
        }

        @Override
        public int getCount() {
            return filesAndFolders.length;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {

            View v = null;
            if (position < 0 || position > getCount() - 1)
                return v;

            if (convertView != null) {
                v = convertView;
                int i = parent.indexOfChild(v);
                if (((File) filesAndFolders[position]).getPath().equals(defaultFilePath)) {
                    v = inflater.inflate(R.layout.bookmark_icon, parent, false);
                } else if (((File) filesAndFolders[position]).isDirectory()) {
                    v = inflater.inflate(R.layout.folder_icon, parent, false);
                } else {
                    v = inflater.inflate(R.layout.file_icon, parent, false);
                }
            } else {
                if (((File) filesAndFolders[position]).getPath().equals(defaultFilePath)) {
                    v = inflater.inflate(R.layout.bookmark_icon, parent, false);
                } else if (((File) filesAndFolders[position]).isDirectory()) {
                    v = inflater.inflate(R.layout.folder_icon, parent, false);
                } else {
                    v = inflater.inflate(R.layout.file_icon, parent, false);
                }
            }

            TextView path = (TextView) ((ViewGroup) v).getChildAt(1);
            if (((File)filesAndFolders[position]).getName().endsWith("..") ||
                ((EmuFile)filesAndFolders[position]).isParent()) {
                path.setText("..");
            } else if (((File) filesAndFolders[position]).getPath().equals(defaultFilePath)
                       && actionType.equals("load_rom")) {
                path.setText("Set as Default Dir");
            } else if (((File) filesAndFolders[position]).getPath().equals(thisDirPath)
                    && actionType.equals("export_states")) {
                path.setText("Export Here");
            }
            else {
                String name = ((File)filesAndFolders[position]).getName();
                if (name.length() > 40)
                    name = name.substring(0, 40 - 3) + "...";
                path.setText(name);
            }

            return v;
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public Object getItem(int position) {
            return filesAndFolders[position];
        }

    }

    /**
     * This class allows us to listen for clicks on items in the
     * GridView.
     */
    private class PathClickListener implements AdapterView.OnItemClickListener {
        @Override
        public void onItemClick(AdapterView av, View v, int position, long id) {
            File file = (File)av.getItemAtPosition(position);
            String filePath = file.getPath().toLowerCase();
            if (actionType.equals("load_rom")) {
                if (file.isDirectory() || filePath.endsWith(".zip")) {
                    populateFileGrid(file.getAbsolutePath());
                } else if (file.getAbsolutePath().equals(defaultFilePath)) {
                    setDefaultPath();
                } else {
                    if (!romClicked) {
                        // prevent double clicks which mess up SDL
                        romClicked = true;

                        // cast File to EmuFile and load ROM data from it
                        EmuFile emuFile = (EmuFile) file;
                        RomData romData = new RomData(emuFile);

                        // check ROM is validly initialised
                        if (!romData.isReady()) {
                            Toast error = Toast.makeText(FileBrowser.this, "Can't open ROM file, error encountered", Toast.LENGTH_SHORT);
                            error.show();
                            return;
                        }
                        FileBrowser.transferData = romData;

                        if (OptionStore.game_genie) {
                            // get CRC32 checksum
                            CRC32 crcGen = new CRC32();
                            crcGen.update(romData.getRomData());
                            FileBrowser.transferChecksum = Long.toHexString(crcGen.getValue());
                            Intent codesIntent = new Intent(FileBrowser.this, CodesActivity.class);
                            startActivity(codesIntent);
                        } else {
                            Intent sdlIntent = new Intent(FileBrowser.this, SDLActivity.class);
                            startActivity(sdlIntent);
                        }

                        finish();
                    }
                }
            }
            else if (actionType.equals("import_states")) {
                if (file.isDirectory()) {
                    populateFileGrid(file.getAbsolutePath());
                } else {
                    // create dialogue
                    importPath = file.getAbsolutePath();
                    AlertDialog importMenu = new AlertDialog.Builder(FileBrowser.this).create();
                    importMenu.setTitle("Import Prompt");
                    importMenu.setMessage("Are you sure you want to import from " +
                            file.getName() + "?");
                    FileBrowser.ImportListener il = new FileBrowser.ImportListener();
                    importMenu.setButton(DialogInterface.BUTTON_POSITIVE, "YES", il);
                    importMenu.setButton(DialogInterface.BUTTON_NEGATIVE, "NO", il);
                    importMenu.show();
                }
            }
            else if (actionType.equals("export_states")) {
                if (file.isDirectory()) {
                    populateFileGrid(file.getAbsolutePath());
                } else {
                    // check for permissions
                    if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.M) {
                        if (ContextCompat.checkSelfPermission(FileBrowser.this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
                                != PackageManager.PERMISSION_GRANTED) { // not granted
                            ActivityCompat.requestPermissions(FileBrowser.this,
                                    new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                                    FILEBROWSER_WRITE_STORAGE_REQUEST);
                        } else {
                            exportMethod();
                        }
                    } else {
                        exportMethod();
                    }
                }
            }
        }
    }

    private void exportMethod() {
        // create dialogue
        AlertDialog exportMenu = new AlertDialog.Builder(FileBrowser.this).create();
        exportMenu.setTitle("Export Prompt");
        exportMenu.setMessage("Are you sure you want to export to " +
                currentPath + "?");
        FileBrowser.ExportListener el = new FileBrowser.ExportListener();
        exportMenu.setButton(DialogInterface.BUTTON_POSITIVE, "YES", el);
        exportMenu.setButton(DialogInterface.BUTTON_NEGATIVE, "NO", el);
        exportMenu.show();
    }

    /**
     * This class allows us to listen for clicks on the internal/external buttons.
     */
    private class StorageClickListener implements View.OnClickListener {
        @Override
        public void onClick(View v) {
            FileBrowser f = FileBrowser.this;
            ControllerTextView t = (ControllerTextView)v;
            String buttonText = t.getText().toString();

            if (buttonText.equals("INTERNAL")) {
                f.populateFileGrid(f.internalStorageRoot);
                ControllerTextView other = (ControllerTextView)f.findViewById(R.id.external_view);
                other.unHighlight();
                t.highlight();
            } else if (buttonText.equals("EXTERNAL")) {
                if (externalStorageRoot != null) {
                    f.populateFileGrid(externalStorageRoot);
                    ControllerTextView other = (ControllerTextView)f.findViewById(R.id.internal_view);
                    other.unHighlight();
                    t.highlight();
                } else {
                    Toast notMounted = Toast.makeText(f, "External SD card not inserted/mounted", Toast.LENGTH_SHORT);
                    notMounted.show();
                }
            }
        }
    }

    /**
     * This method allows us to catch controller events we are interested in - in this case
     * the shoulder buttons - to help us switch between internal and external storage.
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        // Fetch the EmuGridView object, and from it the StorageClickListener, so we can
        // redirect requests there and utilise code sharing.
        boolean returnVal = false;
        if (sc == null || selectionObj == null)
            return returnVal;

        // If the event wasn't consumed and we care about it, consume here.
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            if (event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_L1) {
                if (externalStorageRoot != null) {
                    selectionObj.moveUp();
                    ControllerMapped cm = selectionObj.getSelected();
                    if (cm != null)
                        sc.onClick((ControllerTextView)cm);
                } else {
                    Toast notMounted = Toast.makeText(this, "External SD card not inserted/mounted", Toast.LENGTH_SHORT);
                    notMounted.show();
                }
                returnVal = true;
            } else if (event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_R1) {
                if (externalStorageRoot != null) {
                    selectionObj.moveDown();
                    ControllerMapped cm = selectionObj.getSelected();
                    if (cm != null)
                        sc.onClick((ControllerTextView)cm);
                } else {
                    Toast notMounted = Toast.makeText(this, "External SD card not inserted/mounted", Toast.LENGTH_SHORT);
                    notMounted.show();
                }
                returnVal = true;
            } else if (event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_B) {
                returnVal = true;
                finish();
            } else if (event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_A) {
                g = (EmuGridView)findViewById(R.id.filegrid);
                int position = g.getSelectedItemPosition();
                if (position != AdapterView.INVALID_POSITION) {
                    pc.onItemClick(g, null, position, 0L);
                }
                returnVal = true;
            }
        }

        if (returnVal)
            return returnVal;

        return super.dispatchKeyEvent(event);
    }

    /**
     * This method sets the default path.
     */
    public void setDefaultPath() {
        // define variables
        StringBuilder settings = new StringBuilder();
        boolean errors = false;

        File settingsFile = new File(this.getFilesDir() + "/settings.ini");

        // Open settings file
        if (settingsFile.exists()) {
            BufferedReader settingsReader = null;
            try {
                settingsReader = new BufferedReader(new FileReader(settingsFile));
                String temp;
                while ((temp = settingsReader.readLine()) != null) {
                    if (!temp.startsWith("default_path")) {
                        settings.append(temp + "\n");
                    }
                }
                settings.append("default_path=" + this.currentPath + "\n");
                OptionStore.default_path = this.currentPath;
            } catch (FileNotFoundException e) {
                Log.v("FileBrowser", "Settings file doesn't exist yet: " + e);
            } catch (IOException e) {
                Log.e("FileBrowser", "Problem reading settings file: " + e);
            } finally {
                // Close settings file
                try {
                    if (settingsReader != null)
                        settingsReader.close();
                } catch (IOException e) {
                    Log.e("FileBrowser", "Couldn't close settings file: " + e);
                }
            }

            // set length of file to zero
            RandomAccessFile r = null;
            try {
                r = new RandomAccessFile(settingsFile, "rw");
                r.setLength(0);
            } catch (Exception e) {
                Log.e("FileBrowser", "Problem occurred with RandomAccessFile: " + e);
                errors = true;
            } finally {
                // close random acess file
                try {
                    r.close();
                } catch (IOException e) {
                    Log.e("FileBrowser", "Couldn't close RandomAccessFile: " + e);
                    errors = true;
                }
            }
        }

        // write settings to file
        BufferedWriter settingsWriter = null;
        try {
            settingsWriter = new BufferedWriter(new FileWriter(settingsFile));
            settingsWriter.write(settings.toString(), 0, settings.length());
        }
        catch (Exception e) {
            Log.e("FileBrowser", "Problem occurred writing to settings file: " + e);
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
            status = Toast.makeText(FileBrowser.this, "Failed to set default path", Toast.LENGTH_SHORT);
            status.show();
        } else {
            status = Toast.makeText(FileBrowser.this, "Successfully set default path", Toast.LENGTH_SHORT);
            status.show();
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
     * This is the listener which handles importing of states.
     */
    private class ImportListener implements DialogInterface.OnClickListener {
        @Override
        public void onClick(DialogInterface dialog, int which) {
            switch (which) {
                case DialogInterface.BUTTON_POSITIVE:
                    // import files with StateIO class
                    StateIO manager = new StateIO();
                    boolean result = manager.importFromZip(getFilesDir().getAbsolutePath(), importPath);

                    // check success/failure
                    if (result) {
                        Toast success = Toast.makeText(FileBrowser.this, "Imported states successfully", Toast.LENGTH_SHORT);
                        success.show();
                        finish();
                    } else {
                        Toast failure = Toast.makeText(FileBrowser.this, "Couldn't find states in this file", Toast.LENGTH_SHORT);
                        failure.show();
                    }
                    break;
                case DialogInterface.BUTTON_NEGATIVE:
                    FileBrowser.this.showMessage("Import was cancelled");
                    break;
            }
        }
    }

    /**
     * This is the listener which handles exporting of states.
     */
    private class ExportListener implements DialogInterface.OnClickListener {
        @Override
        public void onClick(DialogInterface dialog, int which) {
            switch (which) {
                case DialogInterface.BUTTON_POSITIVE:
                    // import files with StateIO class
                    StateIO manager = new StateIO();
                    Date currentDate = new Date();
                    Calendar cal = Calendar.getInstance();
                    cal.setTime(currentDate);
                    StringBuilder saveStateFileName = new StringBuilder();
                    saveStateFileName.append("MasterEmu_StateExport_");
                    saveStateFileName.append(cal.get(Calendar.YEAR));
                    saveStateFileName.append('-');
                    saveStateFileName.append(cal.get(Calendar.MONTH) + 1);
                    saveStateFileName.append('-');
                    saveStateFileName.append(cal.get(Calendar.DAY_OF_MONTH));
                    saveStateFileName.append('_');
                    saveStateFileName.append(cal.get(Calendar.HOUR_OF_DAY));
                    saveStateFileName.append('-');
                    saveStateFileName.append(cal.get(Calendar.MINUTE));
                    saveStateFileName.append('-');
                    saveStateFileName.append(cal.get(Calendar.SECOND));
                    saveStateFileName.append(".zip");
                    boolean result = manager.exportToZip(getFilesDir().getAbsolutePath(), FileBrowser.this.currentPath + "/" + saveStateFileName.toString());

                    // check success/failure
                    if (result) {
                        Toast success = Toast.makeText(FileBrowser.this, "Exported states successfully", Toast.LENGTH_SHORT);
                        success.show();
                        finish();
                    } else {
                        Toast failure = Toast.makeText(FileBrowser.this, "Failed to export states here", Toast.LENGTH_SHORT);
                        failure.show();
                    }
                    break;
                case DialogInterface.BUTTON_NEGATIVE:
                    FileBrowser.this.showMessage("Export was cancelled");
                    break;
            }
        }
    }
}
