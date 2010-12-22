<?php
// $Id: messaging_method_mail.class.inc,v 1.1.2.1 2010/06/04 17:57:11 jareyero Exp $
/**
 * @file
 * Drupal Messaging Framework - Send_Method class file
 */

/**
 * Messaging Send Method: Sendgrid mass mailing
 */
class Messaging_Sendgrid_Method extends Messaging_HTML_Mail_Method {
  // Default group and address type
  public $method = 'sendgrid';

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
    // Disable token replacement for message
    $template = $message->get_template();
    // We do token replacement with tokens for available objects (no user or destination)
    // But avoid to clear the ones that don't have a value yet
    $template->set_option('clear', FALSE);
    //$template->set_option('replace', FALSE);
      
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
    $tokens = $this->bulk_tokens($destinations, $template->render('body'), $message->get_language());
    foreach ($tokens as $key => $values) {
      $header->addSubVal($key, array_values($values));
    }
    $mail['headers']['X-SMTPAPI'] = $header->as_string();
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
    $str = wordwrap($json, 76, "\n   ");
    return $str;
  }
}
