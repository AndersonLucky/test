package com.openmobileww.tipcalc;

import java.text.NumberFormat;

import com.darwinsys.android.NumberPickerLogic;
import com.openmobileww.tipcalc.R;
import com.openmobileww.tipcalc.util.IabHelper;
import com.openmobileww.tipcalc.util.IabResult;
import com.openmobileww.tipcalc.util.Inventory;
import com.openmobileww.tipcalc.util.Purchase;

//import allcode.android.accessibilityservice.AccessibilityEvent;
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
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.RadioGroup.OnCheckedChangeListener;
import android.widget.Spinner;
import android.widget.TextView;

public class TipsterActivity extends Activity implements OnItemSelectedListener 
{

	// Debug tag, for logging
	static final String TAG = "Tipster";
	//static final String PREFS_NAME = "PREFS";
	// The helper object
	IabHelper mHelper;

	// SKUs for our products: the premium upgrade (non-consumable) and gas
	// (consumable)
	static final String SKU_RUSSIA = "russia";
//	static final String SKU_TOKEN = "token";

	// (arbitrary) request code for the purchase flow
	static final int RC_REQUEST = 10001;

	// Does the user have Russia tipping?
	//boolean bRussiaTipping = false;
	
	boolean m_selectedCountryRussian = false;

	final static int DEFAULT_NUM_PEOPLE = 3;

	final static NumberFormat formatter = NumberFormat.getCurrencyInstance();

	// Widgets in the application
	private EditText txtAmount;
	private EditText txtPeople;
	private EditText txtTipOther;
	private RadioGroup rdoGroupTips;
	private Button btnCalculate;
//	private int token = 0;
	private Button btnReset;
	private Button btnstoreButton;
	private Spinner spnMain;

	private TextView txtTipAmount;
	private TextView txtTotalToPay;
	private TextView txtTipPerPerson;
	private TextView txtCountRemain;
	private TextView txtTitle;
	private RadioButton rb15PercentUS;
	private RadioButton rb20PercentUS;
	private TextView txtLblCount;
	
	
	// For the id of radio button selected
	private int radioCheckedId = -1;
	private NumberPickerLogic mLogic;

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) 
	{
		Log.i(TAG, "onCreate start.");
		
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main);
		
		m_selectedCountryRussian = getRussinaSelected(); 
		
		// load game data
		loadData();

		String base64EncodedPublicKey = "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA6RgJ3k1jEpHl3XwIvwfGJ9dMVr/ardF+Ev5dKTniocixjnppR1E5LZ5W2MN9oJd5MMolD+4uN4GC3vnEJQJu2662NFwUl+Xh0ulNDCjxnSw/i5WeET18uv3b8ebPOqzkeY/UHv7fpSd1YCeoVCHjNBy74XaRU7GNAgtkR53Q4KjgeNUONEzh+tSvjpWtMpdn9Su7oGLWHsUeDHBeYJACdprBVOMGLxbrFq71fJzwpMCKaw/ga6/htc8XhvMUGwI9OxiTTtsggt/oMhanF6cuQVgQ0KzNWLW63DmVOkq0ktznwZsM5ERjyo9j2PntftBSCMxdGb0f7dlNX8cawGJxUwIDAQAB";
		
		// Create the helper, passing it our context and the public key to
		// verify signatures with
		Log.i(TAG, "Creating IAB helper.");
		mHelper = new IabHelper(this, base64EncodedPublicKey);

		// enable debug logging (for a production application, you should set
		// this to false).
		mHelper.enableDebugLogging(true);

		// Start setup. This is asynchronous and the specified listener
		// will be called once setup completes.
		Log.i(TAG, "Starting setup.");
		
		mHelper.startSetup(new IabHelper.OnIabSetupFinishedListener() 
		{
			public void onIabSetupFinished(IabResult result) 
			{
				Log.i(TAG, "Setup finished.");

				if (!result.isSuccess()) 
				{
					// Oh noes, there was a problem.
					// complain("Problem setting up in-app billing: " + result);
					Log.i(TAG, "Problem setting up in-app billing" + result);
					mHelper.flagEndAsync();
					return;
				}

				// Have we been disposed of in the meantime? If so, quit.
				if (mHelper == null)
				{
					Log.i(TAG, "IabHelper has been disposed of");
					return;
				}

				// IAB is fully set up. Now, let's get an inventory of stuff we
				// own.
				Log.i(TAG, "Setup successful. Querying inventory.");
				mHelper.queryInventoryAsync(mGotInventoryListener);
			}
		}); 

		// Access the various widgets by their id in R.java
		txtAmount = (EditText) findViewById(R.id.txtAmount);
		// On app load, the cursor should be in the Amount field
		//txtAmount.requestFocus();

		txtPeople = (EditText) findViewById(R.id.txtPeople);
		txtPeople.setText(Integer.toString(DEFAULT_NUM_PEOPLE));

		txtTipOther = (EditText) findViewById(R.id.txtTipOther);

		rdoGroupTips = (RadioGroup) findViewById(R.id.RadioGroupTips);

		btnCalculate = (Button) findViewById(R.id.btnCalculate);
		// On app load, the Calculate button is disabled
		btnCalculate.setEnabled(false);
		
		btnReset = (Button) findViewById(R.id.btnReset);

		txtTipAmount = (TextView) findViewById(R.id.txtTipAmount);
		txtTotalToPay = (TextView) findViewById(R.id.txtTotalToPay);
		txtTipPerPerson = (TextView) findViewById(R.id.txtTipPerPerson);
		txtCountRemain = (TextView) findViewById(R.id.txtCountRemain);
		txtLblCount = (TextView) findViewById(R.id.txtLblCount);
		
		txtTitle = (TextView) findViewById(R.id.textView1);;
		rb15PercentUS = (RadioButton) findViewById(R.id.radioFifteen);
		rb20PercentUS = (RadioButton) findViewById(R.id.radioTwenty);
		btnstoreButton = (Button) findViewById(R.id.storeButton);
		
		// On app load, disable the 'Other tip' percentage text field
		txtTipOther.setEnabled(false);

		spnMain = (Spinner) findViewById(R.id.spinnerMain);
		spnMain.setVisibility(View.VISIBLE);
		ArrayAdapter<CharSequence> adapter = ArrayAdapter.createFromResource(
				this, R.array.location, android.R.layout.simple_spinner_item);
		// Specify the layout to use when the list of choices appears
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		// Apply the adapter to the spinner
		spnMain.setAdapter(adapter);
		spnMain.setOnItemSelectedListener(this);
		
		setTipsterCountry(m_selectedCountryRussian);

		/*
		 * Attach a OnCheckedChangeListener to the radio group to monitor radio
		 * buttons selected by user
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
		
	    int countRemain = getRussinaCountRemaining();
		txtCountRemain.setText(Integer.toString(countRemain));
		
		//bRussiaTipping = true;
		updateUi();

	    spnMain.setSelection(((ArrayAdapter<String>)spnMain.getAdapter()).getPosition("Russia"));

		
		Log.i(TAG, "onCreate end.");
	}

	// Listener that's called when we finish querying the items and
	// subscriptions we own
	IabHelper.QueryInventoryFinishedListener mGotInventoryListener = new IabHelper.QueryInventoryFinishedListener() {
		public void onQueryInventoryFinished(IabResult result,
				Inventory inventory) {
			Log.i(TAG, "Query inventory finished.");

			// Have we been disposed of in the meantime? If so, quit.
			if (mHelper == null)
				return;

			// Is it a failure?
			if (result.isFailure()) 
			{
				// complain("Failed to query inventory: " + result);
				Log.i(TAG, "Failed to query inventory: " + result);
				mHelper.flagEndAsync();
				//bRussiaTipping = false;
				updateUi();
				return;
			}

			Log.i(TAG, "Query inventory was successful.");

			/*
			 * Check for items we own. Notice that for each purchase, we check
			 * the developer payload to see if it's correct! See
			 * verifyDeveloperPayload().
			 */

			// Non-consumables
			// Do we have the Russian Tipping?
			Purchase russianTipping = inventory.getPurchase(SKU_RUSSIA);
			boolean bRussiaTipping = (russianTipping != null && verifyDeveloperPayload(russianTipping));
			Log.i(TAG, "User " + (bRussiaTipping ? "HAS" : "DOES NOT HAVE")
					+ " russian tipping.");

			// Consumables
			// Check for token delivery -- if we own token, we should consume it
			// immediately
//			Purchase tokenPurchase = inventory.getPurchase(SKU_TOKEN);
//			if (tokenPurchase != null && verifyDeveloperPayload(tokenPurchase)) {
//				Log.i(TAG, "We have gas. Consuming it.");
//				mHelper.consumeAsync(inventory.getPurchase(SKU_TOKEN),
//						mConsumeFinishedListener);
//				return;
//			}

			updateUi();
			Log.i(TAG, "Initial inventory query finished; enabling main UI.");
		}
	};

	public void onRussianButtonClicked(View view) 
	{
		/*
		 * TODO: for security, generate your payload here for verification. See
		 * the comments on verifyDeveloperPayload() for more info. Since this is
		 * a SAMPLE, we just use an empty string, but on a production app you
		 * should carefully generate this.
		 */
		String payload = "";
        try 
        {
        	Log.i(TAG, "onRussianButtonClicked call launchPurchaseFlow. " + SKU_RUSSIA + " " +  RC_REQUEST );
		    mHelper.launchPurchaseFlow(this, SKU_RUSSIA, RC_REQUEST,
				    mPurchaseFinishedListener, payload);
        } catch (Exception e) 
        {
        	Log.i(TAG, "onRussianButtonClicked ERROR. " + e.toString() );
        	e.printStackTrace(); 
        }

	}

	// Consumable Item
//	public void onTokenButtonClicked(View view) {
		/*
		 * TODO: for security, generate your payload here for verification. See
		 * the comments on verifyDeveloperPayload() for more info. Since this is
		 * a SAMPLE, we just use an empty string, but on a production app you
		 * should carefully generate this.
		 */
//		String payload = "";
//
//		mHelper.launchPurchaseFlow(this, SKU_TOKEN, RC_REQUEST,
//				mPurchaseFinishedListener, payload);
//	}

	/** Verifies the developer payload of a purchase. */
	boolean verifyDeveloperPayload(Purchase p) {
		String payload = p.getDeveloperPayload();

		/*
		 * TODO: verify that the developer payload of the purchase is correct.
		 * It will be the same one that you sent when initiating the purchase.
		 * 
		 * WARNING: Locally generating a random string when starting a purchase
		 * and verifying it here might seem like a good approach, but this will
		 * fail in the case where the user purchases an item on one device and
		 * then uses your app on a different device, because on the other device
		 * you will not have access to the random string you originally
		 * generated.
		 * 
		 * So a good developer payload has these characteristics:
		 * 
		 * 1. If two different users purchase an item, the payload is different
		 * between them, so that one user's purchase can't be replayed to
		 * another user.
		 * 
		 * 2. The payload must be such that you can verify it even when the app
		 * wasn't the one who initiated the purchase flow (so that items
		 * purchased by the user on one device work on other devices owned by
		 * the user).
		 * 
		 * Using your own server to store and verify developer payloads across
		 * app installations is recommended.
		 */

		return true;
	}

	// Callback for when a purchase is finished
	IabHelper.OnIabPurchaseFinishedListener mPurchaseFinishedListener = new IabHelper.OnIabPurchaseFinishedListener() {
		public void onIabPurchaseFinished(IabResult result, Purchase purchase, int responseCode) {
			Log.i(TAG, "Purchase finished: " + result + ", purchase: "
					+ purchase);

			// if we were disposed of in the meantime, quit.
			if (mHelper == null)
				return;

			if (result.isFailure()) {
				// complain("Error purchasing: " + result);
				// setWaitScreen(false);
				Log.i(TAG, "Error purchasing: " + result);
				
				Log.i(TAG, "Error purchasing response: " + Integer.toString(result.getResponse()));
				
				 String message = GetGoogleErrorMessage(result.getResponse());
					alert(message);
					updateUi();
				
				mHelper.flagEndAsync();
				return;
			}
			// if (!verifyDeveloperPayload(purchase)) {
			// complain("Error purchasing. Authenticity verification failed.");
			// return;
			// }

			Log.i(TAG, "Purchase successful.");
			
			Log.i(TAG, "Try Consume.");

			if (purchase.getSku().equals(SKU_RUSSIA)) {
				Log.i(TAG,
						"Purchase is Russian tipping upgrade. Congratulating user.");
				alert("Thank you for purchasing 3 more calculations.");
				//bRussiaTipping = true;
				updateUi();

				Log.i(TAG, "Try Consume2.");
				mHelper.consumeAsync(purchase,
						mConsumeFinishedListener);  
				
//				Purchase tokenPurchase = inventory.getPurchase(SKU_TOKEN);
//				if (tokenPurchase != null && verifyDeveloperPayload(tokenPurchase)) {
//					Log.i(TAG, "We have gas. Consuming it.");
//					mHelper.consumeAsync(inventory.getPurchase(SKU_TOKEN),
//							mConsumeFinishedListener);
				
			}
			
			/*
			if( responseCode != IabHelper.BILLING_RESPONSE_RESULT_OK )
			{
				String message = "";
				
				 switch (responseCode) 
				 {
	                case IabHelper.BILLING_RESPONSE_RESULT_USER_CANCELED: 
	                {
	                	message = getResources().getString(R.string.BILLING_RESPONSE_RESULT_USER_CANCELED);
	                } 
	                case IabHelper.BILLING_RESPONSE_RESULT_BILLING_UNAVAILABLE: 
	                {
	                	message = getResources().getString(R.string.BILLING_RESPONSE_RESULT_BILLING_UNAVAILABLE);
	                }
	                case IabHelper.BILLING_RESPONSE_RESULT_ITEM_UNAVAILABLE: 
	                {
	                	message = getResources().getString(R.string.BILLING_RESPONSE_RESULT_ITEM_UNAVAILABLE);
	                }
	                case IabHelper.BILLING_RESPONSE_RESULT_DEVELOPER_ERROR: 
	                {
	                	message = getResources().getString(R.string.BILLING_RESPONSE_RESULT_DEVELOPER_ERROR);
	                }  
	                case IabHelper.BILLING_RESPONSE_RESULT_ERROR: 
	                {
	                	message = getResources().getString(R.string.BILLING_RESPONSE_RESULT_ERROR);
	                } 
	                case IabHelper.BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED: 
	                {
	                	message = getResources().getString(R.string.BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED);
	                } 
	                case IabHelper.BILLING_RESPONSE_RESULT_ITEM_NOT_OWNED: 
	                {
	                	message = getResources().getString(R.string.BILLING_RESPONSE_RESULT_ITEM_NOT_OWNED);
	                } 
	                default:
	                {
	                	message = "Unknown IAB Error.";
	                }
				
				 } // switch
				 
				
			} // ! OK   */
				
			
//			if (purchase.getSku().equals(SKU_TOKEN)) {
//				Log.i(TAG, "Purchase is token. Starting token consumption.");
//				mHelper.consumeAsync(purchase, mConsumeFinishedListener);
//			}
		}
	};
	
	
	String GetGoogleErrorMessage(int responseCode)
	{
		
		String message = "Unknown IAB Error.";
		
		 switch (responseCode) 
		 {
           case IabHelper.BILLING_RESPONSE_RESULT_USER_CANCELED: 
           {
           	message = getResources().getString(R.string.BILLING_RESPONSE_RESULT_USER_CANCELED);
           	break;
           } 
           case IabHelper.BILLING_RESPONSE_RESULT_BILLING_UNAVAILABLE: 
           {
           	message = getResources().getString(R.string.BILLING_RESPONSE_RESULT_BILLING_UNAVAILABLE);
        	break;
           }
           case IabHelper.BILLING_RESPONSE_RESULT_ITEM_UNAVAILABLE: 
           {
           	message = getResources().getString(R.string.BILLING_RESPONSE_RESULT_ITEM_UNAVAILABLE);
        	break;
           }
           case IabHelper.BILLING_RESPONSE_RESULT_DEVELOPER_ERROR: 
           {
           	message = getResources().getString(R.string.BILLING_RESPONSE_RESULT_DEVELOPER_ERROR);
        	break;
           }  
           case IabHelper.BILLING_RESPONSE_RESULT_ERROR: 
           {
           	message = getResources().getString(R.string.BILLING_RESPONSE_RESULT_ERROR);
        	break;
           } 
           case IabHelper.BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED: 
           {
           	message = getResources().getString(R.string.BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED);
        	break;
           } 
           case IabHelper.BILLING_RESPONSE_RESULT_ITEM_NOT_OWNED: 
           {
           	message = getResources().getString(R.string.BILLING_RESPONSE_RESULT_ITEM_NOT_OWNED);
        	break;
           } 

		
		 } // switch
	
		return message;
		
	}
	

	// Called when consumption is complete
	IabHelper.OnConsumeFinishedListener mConsumeFinishedListener = new IabHelper.OnConsumeFinishedListener() 
	{
		public void onConsumeFinished(Purchase purchase, IabResult result) {
			Log.i(TAG, "Consumption finished. Purchase: " + purchase
					+ ", result: " + result);

			// if we were disposed of in the meantime, quit.
			if (mHelper == null)
				return;

			// We know this is the "token" sku because it's the only one we
			// consume,
			// so we don't check which sku was consumed. If you have more than
			// one
			// sku, you probably should check...
			
			
			if (result.isSuccess()) 
			{
				// successfully consumed, so we apply the effects of the item in
				// app
				Log.i(TAG, "Consumption successful. Provisioning.");
				//bRussiaTipping = false;
				
				 int russianCalcCountRemaining = getRussinaCountRemaining();
			     SharedPreferences settings = getSharedPreferences(IabHelper.PREFS_NAME, 0);
			     russianCalcCountRemaining += 3;
			     SharedPreferences.Editor editor = settings.edit();
			     editor.putInt("countRemain", russianCalcCountRemaining);
			     editor.commit();
			     updateUi();
			     spnMain.setSelection(((ArrayAdapter<String>)spnMain.getAdapter()).getPosition("Russia"));
			    
			} 
			else 
			{
			   String message = GetGoogleErrorMessage( result.getResponse() );
					alert(message);
					updateUi();
				
				complain("Error while consuming: " + result);
				Log.i(TAG, "Error while consuming: " + result);
				mHelper.flagEndAsync();
				//bRussiaTipping = true;
			}
			// setWaitScreen(false);
			Log.i(TAG, "End consumption flow.");
		}
	};

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) 
	{
		Log.i(TAG, "onActivityResult(" + requestCode + "," + resultCode + ","
				+ data);
		
	    if (resultCode == -5) 
	    {
	    	Log.i(TAG, "onActivityResult minus5");
	        return;
	    }
		
		if (mHelper == null)
			return;

		// Pass on the activity result to the helper for handling
		if (!mHelper.handleActivityResult(requestCode, resultCode, data)) {
			// not handled, so handle it ourselves (here's where you'd
			// perform any handling of activity results not related to in-app
			// billing...
			super.onActivityResult(requestCode, resultCode, data);
		} else {
			Log.i(TAG, "onActivityResult handled by IABUtil.");
		}
	}


	// updates UI to reflect model
	public void updateUi() 
	{
		 // "Get tipping in Russia" button is only visible if the user is not
		 // subscribed yet
		
		 int russianCalcCountRemaining = getRussinaCountRemaining();

		 txtCountRemain.setText(Integer.toString(russianCalcCountRemaining));
		
		 Log.i(TAG, "updateUi RussiaTipping: " + ( (russianCalcCountRemaining > 0) ? "TRUE" : "FALSE") );
		 
		 /*
		 findViewById(R.id.storeButton).setVisibility(
				(russianCalcCountRemaining > 0) ? View.GONE : View.VISIBLE); */
		 
		 findViewById(R.id.btnCalculate).setEnabled(
					(russianCalcCountRemaining > 0) ? true : false);

         //	btnCalculate.setText("Compute (" + token + ")");

		 //spnMain.setVisibility( (russianCalcCountRemaining > 0) ? View.VISIBLE : View.GONE);
		
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
		public boolean onKey(View v, int keyCode, KeyEvent event) 
		{
			int russianCalcCountRemaining = getRussinaCountRemaining();
			boolean bEnableOK =   !m_selectedCountryRussian || russianCalcCountRemaining > 0;

			switch (v.getId()) 
			{
			case R.id.txtAmount:
			case R.id.txtPeople:
//				if (token != 0) {
					btnCalculate.setEnabled(bEnableOK 
							&& txtAmount.getText().length() > 0
							&& txtPeople.getText().length() > 0);
//				}
				break;
			case R.id.txtTipOther:
//				if (token != 0) {
					btnCalculate.setEnabled(bEnableOK 
							&&txtAmount.getText().length() > 0
							&& txtPeople.getText().length() > 0
							&& txtTipOther.getText().length() > 0);
//				}
				break;
			}
			mHelper.flagEndAsync();
			return false;
		}

	};

	/**
	 * ClickListener for the Calculate and Reset buttons. Depending on the
	 * button clicked, the corresponding method is called.
	 */
	private OnClickListener mClickListener = new OnClickListener()
	{

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
	private void calculate() 
	{
	   int russianCalcCountRemaining = getRussinaCountRemaining();
	   
	   if( !m_selectedCountryRussian || russianCalcCountRemaining > 0)
	   {
		   
		String sTest = txtAmount.getText().toString().trim();
		if( sTest.length() < 1 )
			return;
		
		sTest = txtPeople.getText().toString().trim();
		if( sTest.length() < 1 )
			return;
		  
		Double billAmount = Double.parseDouble(txtAmount.getText().toString());
		Double totalPeople = Double.parseDouble(txtPeople.getText().toString());
		Double percentage = null;
		boolean isError = false;
		if (billAmount < 1.0) 
		{
			showErrorAlert("Enter a valid Total Amount.", txtAmount.getId());
			isError = true;
		}

		if (totalPeople < 1.0) 
		{
			showErrorAlert("Enter a valid number of people.", txtPeople.getId());
			isError = true;
		}

		/*
		 * If user never changes radio selection, then it means the default
		 * selection of 15% is in effect. But it's safer to verify...
		 */
		if (radioCheckedId == -1) 
		{
			radioCheckedId = rdoGroupTips.getCheckedRadioButtonId();
		}
		if (radioCheckedId == R.id.radioFifteen) 
		{
			percentage = (m_selectedCountryRussian) ? 5.00 : 15.00;
		} 
		else if (radioCheckedId == R.id.radioTwenty) 
		{
			percentage = (m_selectedCountryRussian) ? 10.00 : 20.00;
		} 
		else if (radioCheckedId == R.id.radioOther) 
		{
			percentage = Double.parseDouble(txtTipOther.getText().toString());
			if (percentage < 1.0) 
			{
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
			
			if(m_selectedCountryRussian) 
			{
			     --russianCalcCountRemaining;
				 SharedPreferences settings = getSharedPreferences(IabHelper.PREFS_NAME, 0);
			     SharedPreferences.Editor editor = settings.edit();
			     editor.putInt("countRemain", russianCalcCountRemaining);
			     editor.commit();
			}
		 } // isError
		
		updateUi();
		
	   } 
	}
	

	void saveData() {

		/*
		 * WARNING: on a real application, we recommend you save data in a
		 * secure way to prevent tampering. For simplicity in this sample, we
		 * simply store the data using a SharedPreferences.
		 */

//		SharedPreferences.Editor spe = getPreferences(MODE_PRIVATE).edit();
//		spe.putInt("token", token);
//		spe.commit();
//		Log.i(TAG, "Saved data: token = " + String.valueOf(token));
	}

	void loadData() {
//		SharedPreferences sp = getPreferences(MODE_PRIVATE);
//		token = sp.getInt("token", 1);
//		Log.i(TAG, "Loaded data: token = " + String.valueOf(token));
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
		Log.e(TAG, "**** Tipster Error: " + message);
		alert("Error: " + message);
	}

	void alert(String message) {
		AlertDialog.Builder bld = new AlertDialog.Builder(this);
		bld.setMessage(message);
		bld.setNeutralButton("OK", null);
		Log.i(TAG, "Showing alert dialog: " + message);
		bld.create().show();
	}
	
	

	private void setTipsterCountry(boolean bRussian)
	{
		m_selectedCountryRussian = bRussian;
		
		String mystring15;
		String mystring20;
		String title;
		
		if(bRussian)
		{
			mystring15 = getResources().getString(R.string.rdoTxt5);
			mystring20 = getResources().getString(R.string.rdoTxt10);
			title = getResources().getString(R.string.TITLE_RUSSIA);
			txtLblCount.setVisibility(View.VISIBLE);
			txtCountRemain.setVisibility(View.VISIBLE); 
			btnstoreButton.setVisibility(View.VISIBLE); 
			
		}
		else
		{
			mystring15 = getResources().getString(R.string.rdoTxt15);
			mystring20 = getResources().getString(R.string.rdoTxt20);
			title = getResources().getString(R.string.TITLE_US);
			txtLblCount.setVisibility(View.INVISIBLE);
			txtCountRemain.setVisibility(View.INVISIBLE);
			btnstoreButton.setVisibility(View.GONE);
		}
		
		txtAmount.setText("");
        txtTitle.setText(title);
        rb15PercentUS.setText(mystring15);
        rb20PercentUS.setText(mystring20);   
        
	}
	

	@Override
	public void onItemSelected(AdapterView<?> parentView, View v, int position,
			long id) 
	{
		boolean bRussianSelected = parentView.getItemAtPosition(position).toString().equals("Russia");
		
		setTipsterCountry(bRussianSelected);
		setRussinaSelected(bRussianSelected);
	}
	

	@Override
	public void onNothingSelected(AdapterView<?> parentView) 
	{
		// TODO Auto-generated method stub

	}
	
	
	private int getRussinaCountRemaining() 
	{
		int count = 0;
		SharedPreferences settings = getSharedPreferences(IabHelper.PREFS_NAME, 0);
		if( settings != null)
		{
			count = settings.getInt("countRemain", 0);
		}
		
		return count; 
	}
	
	
	private boolean getRussinaSelected() 
	{
		boolean bRussianSelected = false;
		SharedPreferences settings = getSharedPreferences(IabHelper.PREFS_NAME, 0);
		if( settings != null)
		{
			bRussianSelected = settings.getBoolean("russinaSelected", false);
		}
		
		return bRussianSelected; 
	}
	
	
	private  void setRussinaSelected(boolean bRussianSelected) 
	{
		SharedPreferences settings = getSharedPreferences(IabHelper.PREFS_NAME, 0);
		if( settings != null)
		{
		    SharedPreferences.Editor editor = settings.edit();
		    editor.putBoolean("russinaSelected", bRussianSelected);
		    editor.commit();
		}
		

	}
	


	
	
}