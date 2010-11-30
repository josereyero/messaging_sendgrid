<?php
// $Id: messaging_method_mail.class.inc,v 1.1.2.1 2010/06/04 17:57:11 jareyero Exp $
/**
 * @file
 * Drupal Messaging Framework - Send_Method class file
 */

/**
 * Messaging Send Method: Sendgrid mass mailing
 */
class Messaging_Sendgrid_Method extends Messaging_Mail_Method {
  // Default group and address type
  public $method = 'sendgrid';
  public $type = 'mail';
  public $anonymous = TRUE;
  public $format = MESSAGING_FORMAT_HTML;
  // Mail system object
  protected static $mail_system;

  /**
   * Prepare template for this method.
   * 
   * This is a bulk sending method so we disable token replacement for 'header' and 'footer'
   */  
  function message_render($message) {
    $template = clone $message->get_template();
    $template->set_option('clear', FALSE);
    $template->build();
    $template->set_option('replace', FALSE);
    return $template;
  }
  /**
   * Send message to multiple destinations
   * 
   * @todo For single destination, things could go faster
   */
  function send_multiple($destinations, $message) {
    $results = array();
    // In this case we use bulk sending features and fill in the results with the same value
    $mail = $this->bulk_build($destinations, $message);
    $result = $this->mail_send($mail, $message);
    $results = array_combine(array_keys($destinations), array_fill(0, count($destinations), $result));
    return $results;
  }

  /**
   * Build bulk mail for multiple addresses
   * 
   * Add the Sendgrid custom header which holds multiple recipient info and token replacement information
   */
  function bulk_build($destinations, $message) {
    // First build a message using the default mail as 'to' address
    $defaults = $this->default_params();
    $mail = $this->mail_build($defaults['default_from'], $message);
    // Add recipients and tokens for replacement using Sendgrid headers
    $recipients = $replaces = array();
    foreach ($destinations as $destination) {
      $recipients[] = $destination->get_address();
    }
    $header = new Messaging_Sendgrid_SmtpApiHeader();
    $header->addTo($recipients);
    $tokens = $this->bulk_tokens($destinations, $message->get_body(), $message->get_language());
    foreach ($tokens as $key => $values) {
      $header->addSubVal($key, array_values($values));
    }
    $mail['headers']['X-SMTPAPI'] = $header->asJSON();
    return $mail;
  }
  
  /**
   * Build tokens for replacement
   * 
   * @return array
   *   Tokens indexed by token name and destination index
   */
  function bulk_tokens($destinations, $text, $language = NULL) {
    $text_tokens = token_scan($text);
    $user_tokens = array();
    $options = array(
      'language' => $language ? $language : language_default(),
      'sanitize' => TRUE,
    );
    foreach ($destinations as $index => $destination) {
      $objects = array(
        'destination' => $destination,
        'user' => $destination->get_user()
      );

      foreach ($text_tokens as $type => $tokens) {
        $type_tokens = token_generate($type, $tokens, $objects, $options);
        $type_tokens += array_combine($tokens, $tokens);
        foreach ($type_tokens as $name => $value) {
          $user_tokens[$name][$index] = $value;
        }
      }
    }
    return $user_tokens;
  }
  
  /**
   * Actually send mail through Drupal system
   * 
   * @param $mail
   *   Built Drupal mail array, not rendered
   * @param $message
   *   Original message object
   */
  protected static function mail_send($mail, $message) {
    // Retrieve the responsible implementation for this message.
    //$system = drupal_mail_system($mail['module'], $mail['key']);
    if (!isset(self::$mail_system)) {
      self::$mail_system = new Messaging_Sendgrid_MailSystem();
    }
    // Format the message body, last chance for formatting
    $mail = self::$mail_system->format($mail);
    return self::$mail_system->mail($mail);    
  }

  /**
   * Get mail headers. Helper function for mail methods
   * 
   * Just add some headers, but reuse most of default headers from parent 
   */
  static function mail_headers($message, $params = array()) {
    $params += array('headers' => array());
    $params['headers'] += array(
      'MIME-Version'              => '1.0',
      'Content-Type'              => 'text/html; charset=UTF-8; format=flowed; delsp=yes',
    );
    return parent::mail_headers($message, $params);
  }
}

/**
 * SmtpApiHeader from http://wiki.sendgrid.com/doku.php?id=smtpapiheader.php
 */
# Version 1.0
# Last Updated 6/22/2009

class Messaging_Sendgrid_SmtpApiHeader
{
  var $data; 
 
  function addTo($tos)
  {
    if (!isset($this->data['to'])) 
    {
      $this->data['to'] = array();
    }
    $this->data['to'] = array_merge($this->data['to'], (array)$tos);
  }
 
  function addSubVal($var, $val)
  {
    if (!isset($this->data['sub'])) 
    {
      $this->data['sub'] = array();
    }
 
    if (!isset($this->data['sub'][$var])) 
    {
      $this->data['sub'][$var] = array();
    }
    $this->data['sub'][$var] = array_merge($this->data['sub'][$var], (array)$val);
  }
 
 
  function setUniqueArgs($val)
  {
    if (!is_array($val)) return;
    // checking for associative array
    $diff = array_diff_assoc($val, array_values($val));
    if(((empty($diff)) ? false : true))
    {
      $this->data['unique_args'] = $val;
    } 
  }
 
  function setCategory($cat)
  {
    $this->data['category'] = $cat;
  }
 
  function addFilterSetting($filter, $setting, $value)
  {
    if (!isset($this->data['filters'])) 
    {
      $this->data['filters'] = array();
    }
 
    if (!isset($this->data['filters'][$filter])) 
    {
      $this->data['filters'][$filter] = array();
    }
 
    if (!isset($this->data['filters'][$filter]['settings'])) 
    {
      $this->data['filters'][$filter]['settings'] = array();
    }
    $this->data['filters'][$filter]['settings'][$setting] = $value;
  }
 
  function asJSON()
  {
    $json = json_encode($this->data);
    // Add spaces so that the field can be foldd
    $json = preg_replace('/(["\]}])([,:])(["\[{])/', '$1$2 $3', $json);
    return $json;
  }
 
  function as_string()
  {
    $json = $this->asJSON();
    $str = "X-SMTPAPI: " . wordwrap($json, 76, "\n   ");
    return $str;
  }
}

/**
 * Sendgrid mail system
 * 
 * We use HTML formatting + we need to take care of properly filtering the headers.
 * 
 * The e-mail is sent through default PHP mail though, Sendgrid config should be in the backend
 * 
 * For Exim configuration see http://wiki.sendgrid.com/doku.php?id=exim4
 * 
 * @todo Explore other options for sending, like PHPMailer or
 */
class Messaging_Sendgrid_MailSystem extends DefaultMailSystem {
  /**
   * Concatenate and wrap the e-mail body for plain-text mails.
   *
   * @param $message
   *   A message array, as described in hook_mail_alter().
   *
   * @return
   *   The formatted $message.
   */
  public function format(array $message) {
    // Join the body array into one string.
    $message['body'] = implode("<br/>\n", $message['body']);
    // Convert any HTML to plain-text.
    // $message['body'] = drupal_html_to_text($message['body']);
    // Wrap the mail body for sending.
    // $message['body'] = drupal_wrap_mail($message['body']);
    return $message;
  }

  /**
   * Send an e-mail message, using Drupal variables and default settings.
   *
   * @see http://php.net/manual/en/function.mail.php
   * @see drupal_mail()
   *
   * @param $message
   *   A message array, as described in hook_mail_alter().
   * @return
   *   TRUE if the mail was successfully accepted, otherwise FALSE.
   */
  public function mail(array $message) {
    // If 'Return-Path' isn't already set in php.ini, we pass it separately
    // as an additional parameter instead of in the header.
    // However, if PHP's 'safe_mode' is on, this is not allowed.
    if (isset($message['headers']['Return-Path']) && !ini_get('safe_mode')) {
      $return_path_set = strpos(ini_get('sendmail_path'), ' -f');
      if (!$return_path_set) {
        $message['Return-Path'] = $message['headers']['Return-Path'];
        unset($message['headers']['Return-Path']);
      }
    }
    $mimeheaders = array();
    foreach ($message['headers'] as $name => $value) {
      $mimeheaders[] = $name . ': ' . mime_header_encode($value);
    }
    $line_endings = variable_get('mail_line_endings', MAIL_LINE_ENDINGS);
    // Prepare mail commands.
    $mail_subject = mime_header_encode($message['subject']);
    // Note: e-mail uses CRLF for line-endings. PHP's API requires LF
    // on Unix and CRLF on Windows. Drupal automatically guesses the
    // line-ending format appropriate for your system. If you need to
    // override this, adjust $conf['mail_line_endings'] in settings.php.
    $mail_body = preg_replace('@\r?\n@', $line_endings, $message['body']);
    // For headers, PHP's API suggests that we use CRLF normally,
    // but some MTAs incorrectly replace LF with CRLF. See #234403.
    $mail_headers = join("\n", $mimeheaders);
    if (isset($message['Return-Path']) && !ini_get('safe_mode')) {
      $mail_result = mail(
        $message['to'],
        $mail_subject,
        $mail_body,
        $mail_headers,
        // Pass the Return-Path via sendmail's -f command.
        '-f ' . $message['Return-Path']
      );
    }
    else {
      // The optional $additional_parameters argument to mail() is not allowed
      // if safe_mode is enabled. Passing any value throws a PHP warning and
      // makes mail() return FALSE.
      $mail_result = mail(
        $message['to'],
        $mail_subject,
        $mail_body,
        $mail_headers
      );
    }
    return $mail_result;
  }  
}