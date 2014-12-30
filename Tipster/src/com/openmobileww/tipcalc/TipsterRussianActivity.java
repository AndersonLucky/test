package com.openmobileww.tipcalc;

import java.text.NumberFormat;

import com.darwinsys.android.NumberPickerLogic;
import com.openmobileww.tipcalc.R;
import com.openmobileww.tipcalc.util.IabHelper;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnKeyListener;
import android.view.ViewManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioGroup;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.RadioGroup.OnCheckedChangeListener;

public class TipsterRussianActivity extends Activity implements OnItemSelectedListener{
	// Debug tag, for logging
    static final String TAG = "Tipster";
	
    // 	The helper object
    IabHelper mHelper;
	
    // SKUs for our products: the premium upgrade (non-consumable) and gas (consumable)
    static final String SKU_RUSSIA = "russia";
    
    // Does the user have an Russian tipping?
    boolean bRussiaTipping = false;
    
    
    
	final static int DEFAULT_NUM_PEOPLE = 3;
	
	final static NumberFormat formatter =
			NumberFormat.getCurrencyInstance();
	
    // Widgets in the application
    private EditText txtAmount;
    private EditText txtPeople;
    private EditText txtTipOther;
    private RadioGroup rdoGroupTips;
    private Button btnCalculate;
    private Button btnReset;
    
    private Spinner spnRussian;
 
    private TextView txtTipAmount;
    private TextView txtTotalToPay;
    private TextView txtTipPerPerson;
	private TextView txtCountRemain;
 
    // For the id of radio button selected
    private int radioCheckedId = -1;
	private NumberPickerLogic mLogic;
	
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.russianmain);
		
		
		// Access the various widgets by their id in R.java
        txtAmount = (EditText) findViewById(R.id.txtAmount);
        //On app load, the cursor should be in the Amount field  
        txtAmount.requestFocus();
       
        txtPeople = (EditText) findViewById(R.id.txtPeople);
        txtPeople.setText(Integer.toString(DEFAULT_NUM_PEOPLE));

        txtTipOther = (EditText) findViewById(R.id.txtTipOther);
 
        rdoGroupTips = (RadioGroup) findViewById(R.id.RadioGroupTips);
 
        btnCalculate = (Button) findViewById(R.id.btnCalculate);
        //On app load, the Calculate button is disabled
        btnCalculate.setEnabled(false);
 
        btnReset = (Button) findViewById(R.id.btnReset);
 
        txtTipAmount = (TextView) findViewById(R.id.txtTipAmount);
        txtTotalToPay = (TextView) findViewById(R.id.txtTotalToPay);
        txtTipPerPerson = (TextView) findViewById(R.id.txtTipPerPerson);
		txtCountRemain = (TextView) findViewById(R.id.txtCountRemain);
 
        // On app load, disable the 'Other tip' percentage text field
        txtTipOther.setEnabled(false);
		
		
        spnRussian = (Spinner) findViewById(R.id.spinnerRussian);
        
        ArrayAdapter<CharSequence> adapter = ArrayAdapter.createFromResource(this,
                R.array.location, android.R.layout.simple_spinner_item);
    	// Specify the layout to use when the list of choices appears
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        // Apply the adapter to the spinner
        spnRussian.setAdapter(adapter);
        spnRussian.setSelection(1);
        spnRussian.setOnItemSelectedListener(this);
        
        /*
         * Attach a OnCheckedChangeListener to the
         * radio group to monitor radio buttons selected by user
         */
        rdoGroupTips.setOnCheckedChangeListener(new OnCheckedChangeListener() {

        	@Override
        	public void onCheckedChanged(RadioGroup group, int checkedId) {
        		// Enable/disable Other Percentage tip field
        		if (checkedId == R.id.radioFifteen
        				|| checkedId == R.id.radioTwenty) {
        			txtTipOther.setEnabled(false);
        			/*
        			 * Enable the calculate button if Total Amount and No. of
        			 * People fields have valid values.
        			 */
        			btnCalculate.setEnabled(txtAmount.getText().length() > 0
        					&& txtPeople.getText().length() > 0);
        		}
        		if (checkedId == R.id.radioOther) {
        			// enable the Other Percentage tip field
        			txtTipOther.setEnabled(true);
        			// set the focus to this field
        			txtTipOther.requestFocus();
        			/*
        			 * Enable the calculate button if Total Amount and No. of
        			 * People fields have valid values. Also ensure that user
        			 * has entered a Other Tip Percentage value before enabling
        			 * the Calculate button.
        			 */
        			btnCalculate.setEnabled(txtAmount.getText().length() > 0
        					&& txtPeople.getText().length() > 0
        					&& txtTipOther.getText().length() > 0);
        		}
        		// To determine the tip percentage choice made by user
        		radioCheckedId = checkedId;
        	}
        });
        
        
        /*
         * Attach a KeyListener to the Tip Amount, No. of People and Other Tip
         * Percentage text fields
         */
        txtAmount.setOnKeyListener(mKeyListener);
        txtPeople.setOnKeyListener(mKeyListener);
        txtTipOther.setOnKeyListener(mKeyListener);
        
        btnCalculate.setOnClickListener(mClickListener);
        btnReset.setOnClickListener(mClickListener);
        
        /** Create a NumberPickerLogic to handle the + and - keys */
        mLogic = new NumberPickerLogic(txtPeople, 1, Integer.MAX_VALUE);
	}
	
	/*
	 * KeyListener for the Total Amount, No of People and Other Tip Percentage
	 * fields. We need to apply this key listener to check for following
	 * conditions:
	 * 
	 * 1) If user selects Other tip percentage, then the other tip text field
	 * should have a valid tip percentage entered by the user. Enable the
	 * Calculate button only when user enters a valid value.
	 * 
	 * 2) If user does not enter values in the Total Amount and No of People, we
	 * cannot perform the calculations. Hence enable the Calculate button only
	 * when user enters a valid values.
	 */
	private OnKeyListener mKeyListener = new OnKeyListener() {
		@Override
		public boolean onKey(View v, int keyCode, KeyEvent event) {

			switch (v.getId()) {
			case R.id.txtAmount:
			case R.id.txtPeople:
				btnCalculate.setEnabled(txtAmount.getText().length() > 0
						&& txtPeople.getText().length() > 0);
				break;
			case R.id.txtTipOther:
				btnCalculate.setEnabled(txtAmount.getText().length() > 0
						&& txtPeople.getText().length() > 0
						&& txtTipOther.getText().length() > 0);
				break;
			}
			return false;
		}

	};
	
	/**
	 * ClickListener for the Calculate and Reset buttons. Depending on the
	 * button clicked, the corresponding method is called.
	 */
	private OnClickListener mClickListener = new OnClickListener() {

		@Override
		public void onClick(View v) {
			if (v.getId() == R.id.btnCalculate) {
				calculate();
			} else {
				reset();
			}
		}
	};

	/**
	 * Resets the results text views at the bottom of the screen as well as
	 * resets the text fields and radio buttons.
	 */
	private void reset() {
		txtTipAmount.setText("");
		txtTotalToPay.setText("");
		txtTipPerPerson.setText("");
		txtAmount.setText("");
		txtPeople.setText(Integer.toString(DEFAULT_NUM_PEOPLE));
		txtTipOther.setText("");
		rdoGroupTips.clearCheck();
		rdoGroupTips.check(R.id.radioFifteen);
		// set focus on the first field
		txtAmount.requestFocus();
	}

	public void decrement(View v) {
		mLogic.decrement();
	}

	public void increment(View v) {
		mLogic.increment();
	}

	/**
	 * Calculate the tip as per data entered by the user.
	 */
	private void calculate() {
		Double billAmount = Double.parseDouble(txtAmount.getText().toString());
		Double totalPeople = Double.parseDouble(txtPeople.getText().toString());
		Double percentage = null;
		boolean isError = false;
		if (billAmount < 1.0) {
			showErrorAlert("Enter a valid Total Amount.", txtAmount.getId());
			isError = true;
		}

		if (totalPeople < 1.0) {
			showErrorAlert("Enter a valid number of people.", txtPeople.getId());
			isError = true;
		}

		/*
		 * If user never changes radio selection, then it means the default
		 * selection of 15% is in effect. But it's safer to verify...
		 */
		if (radioCheckedId == -1) {
			radioCheckedId = rdoGroupTips.getCheckedRadioButtonId();
		}
		if (radioCheckedId == R.id.radioFifteen) {
			percentage = 5.00;
		} else if (radioCheckedId == R.id.radioTwenty) {
			percentage = 10.00;
		} else if (radioCheckedId == R.id.radioOther) {
			percentage = Double.parseDouble(txtTipOther.getText().toString());
			if (percentage < 1.0) {
				showErrorAlert("Enter a valid Tip percentage",
						txtTipOther.getId());
				isError = true;
			}
		}
		/*
		 * If all fields are populated with valid values, then proceed to
		 * calculate the tips
		 */
		if (!isError) 
		{
			double tipAmount = ((billAmount * percentage) / 100);
			double totalToPay = billAmount + tipAmount;
			double perPersonPays = totalToPay / totalPeople;

			txtTipAmount.setText(formatter.format(tipAmount));
			txtTotalToPay.setText(formatter.format(totalToPay));
			txtTipPerPerson.setText(formatter.format(perPersonPays));
			
		    SharedPreferences settings = getSharedPreferences(IabHelper.PREFS_NAME, 0);
		    int russianCalcCountRemaining = settings.getInt(IabHelper.PREFS_RUSSIAN_COUNT_REMAINING, 0);
		    --russianCalcCountRemaining;
			 
		    SharedPreferences.Editor editor = settings.edit();
		    editor.putInt("countRemain", russianCalcCountRemaining);
		    editor.commit();
		    
			txtCountRemain.setText(Integer.toString(russianCalcCountRemaining));
		    
		}
	}

	/**
	 * Shows the error message in an alert dialog
	 * 
	 * @param errorMessage
	 *            String the error message to show
	 * @param fieldId
	 *            the Id of the field which caused the error. This is required
	 *            so that the focus can be set on that field once the dialog is
	 *            dismissed.
	 */
	private void showErrorAlert(String errorMessage, final int fieldId) {
		new AlertDialog.Builder(this)
				.setTitle("Error")
				.setMessage(errorMessage)
				.setNeutralButton("Close",
						new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface dialog,
									int which) {
								findViewById(fieldId).requestFocus();
							}
						}).show();
	}

	void complain(String message) {
		Log.e(TAG, "**** TrivialDrive Error: " + message);
		alert("Error: " + message);
	}

	void alert(String message) {
		AlertDialog.Builder bld = new AlertDialog.Builder(this);
		bld.setMessage(message);
		bld.setNeutralButton("OK", null);
		Log.d(TAG, "Showing alert dialog: " + message);
		bld.create().show();
	}

	@Override
	public void onItemSelected(AdapterView<?> parentView, View v, int position,
			long id) {
		// TODO Auto-generated method stub
		if(parentView.getItemAtPosition(position).toString().equals("United States"))
		{ 
			Intent returnIntent = new Intent();
			setResult(-5, returnIntent);
			finish();
		}
	}

	@Override
	public void onNothingSelected(AdapterView<?> parentView) {
		// TODO Auto-generated method stub
		
	}
}
