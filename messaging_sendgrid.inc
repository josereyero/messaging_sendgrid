<?php
// $Id: messaging_method_mail.class.inc,v 1.1.2.1 2010/06/04 17:57:11 jareyero Exp $
/**
 * @file
 * Drupal Messaging Framework - Send_Method class file
 */

/**
 * Sendgrid mass mailing
 */
class Messaging_Sendgrid_Method extends Messaging_Mail_Method {
  // Default group and address type
  public $method = 'sendgrid';
  public $type = 'mail';
  public $anonymous = TRUE;
  public $format = MESSAGING_FORMAT_PLAIN;

  /**
   * Send message to multiple destinations
   */
  function send_multiple($destinations, $message) {
    $results = array();
    if (count($destinations) == 1) {
      $key = key($destinations);
      $destination = reset($destinations);
      $results[$key] = $this->send_destination($destination, $message);
    }
    else {
      // In this case we use bulk sending features and fill in the results with the same value
      $mail = $this->bulk_build($destinations, $message);
      $result = $this->mail_send($mail, $message);
      $results = array_combine(array_keys($destinations), array_fill(0, count($destinations), $result));
    }
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
    // Add recipients and names using Sendgrid headers
    $header = new Messaging_Sendgrid_SmtpApiHeader();
    $recipients = $names = array();
    foreach ($destinations as $destination) {
      $recipients[] = $destination->get_address();
      $names[] = $destination->get_name();
    }
    // We'll do token replacement with '-name-'
    $header->addTo($recipients);
    $header->addSubVal('-name-', $names);
    $mail['headers']['X-SMTPAPI'] = $header->asJSON();
    return $mail;
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
    $system = drupal_mail_system($mail['module'], $mail['key']);
    // Format the message body, last chance for formatting
    $mail = $system->format($mail);
    return $system->mail($mail);    
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
